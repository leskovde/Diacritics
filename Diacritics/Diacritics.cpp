#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _CRT_SECURE_NO_WARNINGS
#define STDIO_EXPERIMENTAL 1

#include <iostream>
#include <fstream>
#include <string>
#include <codecvt>
#include <algorithm>
#include <cstdint>
#include <thread>
#include <map>
#include <cstdio>
#include <windows.h>

#include <unordered_map>
#include <set>

#include "WordStructures.h"
#include "LookupStructures.h"
#include "WideCharUtilities.h"
#include "ConflictHandler.h"
#include "Instrumentation.h"
#include <mutex>
#include <atomic>
#include <future>
#include <chrono>
#include <utility>
#include <cassert>
#include "CorpusParser.h"
#include "zlib/zlib.h"
#include "Externals.h"
#include "ErrorHandler.h"
#include "BinaryReader.h"
#include "DataPreparation.h"

#pragma execution_character_set("utf-8")

using namespace std::chrono_literals;
using word_wrapper = std::list<std::wstring*>;

static std::mutex result_words_mutex,
                  foreign_words_mutex;


#ifdef DEBUG
void* operator new(size_t size)
{
	std::wcerr << L"Allocating " << size << L" bytes\n";

	return malloc(size);
}

void operator delete(void* memory, size_t size)
{
	std::wcerr << L"Freeing " << size << L" bytes\n";

	free(memory);
}
#endif

namespace dia
{
	/**
	 * std::wifstream wrapper with file name added
	 */
	struct wifstream final : std::wifstream
	{
	private:
		const std::string file_name_;

	public:
		
		explicit wifstream(const std::string& file_name) : std::wifstream(file_name, binary), file_name_(file_name) { }

		wifstream(const wifstream&) { }
		wifstream(wifstream&&) noexcept { }
		wifstream& operator=(const wifstream&) = delete;
		wifstream& operator=(wifstream&&) = delete;
		~wifstream() = default;
		
		/**
		 * @return The name of the file from which the wifstream was initialized
		 */
		std::string get_file_name() const
		{
			return file_name_;
		}
	};
}


class text_processor;

/**
 * Privately stores user option flags
 */
class user_options
{
	bool silence_ = false;
	bool conflict_ = false;
	bool mem_map_ = false;

	friend class text_processor;

public:
	
	user_options(const bool silence, const bool conflict)
	{
		this->silence_ = silence;
		this->conflict_ = conflict;
	}
};

class processor_state
{
	std::wstring first_w_with_format_, second_w_with_format_, third_w_with_format_;
	std::wstring carry_over_word_;

	std::map<int, const std::wstring> result_words_;
	std::map<int, const std::wstring> result_formats_;

	std::set<std::wstring> potentially_foreign_words_;
	std::vector<std::future<void>> word_futures_;

	friend class text_processor;
};

/**
 * Diacritic adding text processor\n
 * The instance has its own word mapping and model
 */
class text_processor
{
	offset_table ot_;
	word_mapping wm_;
	processor_state s_;
	user_options opt_ = user_options(true, false);
	mem_map mm_ = mem_map(model_name);

public:

	explicit text_processor(user_options& opt) : ot_(load_compressed_model(offset_model_name)),
	                                             wm_(load_word_mapping(dictionary_name))
	{
		opt_ = opt;
	}

	/**
	 * Reads the model file and searches for a variant of words passed via arguments
	 *
	 * @param second_w_mapped The word in question - to have diacritics added to it
	 * @param first_w_mapped The word directly preceding the word in question in text (optional)
	 * @param third_w_mapped The word directly following the word in question in text (optional)
	 * @param variant_map word, tuple or triplet map to which every satisfactory combination will be added
	 */
	template <typename T>
	void search_model_for(std::map<int, std::vector<T>>& variant_map, const int second_w_mapped, int first_w_mapped = 0,
	                      int third_w_mapped = 0)
	{
		PROFILE_FUNCTION();

		std::unique_ptr<binary_reader> mif;

		if (opt_.mem_map_)
		{
			mif = std::unique_ptr<binary_reader>(std::make_unique<mmap_binary_reader>(mm_));
		}
		else
		{
			mif = std::unique_ptr<binary_reader>(std::make_unique<ifstream_binary_reader>(model_name));
		}
		
		auto arg_count = 3;

		if (first_w_mapped == 0)
			arg_count = 1;
		else if (third_w_mapped == 0)
			arg_count = 2;

		const size_t offset = ot_.get_value(second_w_mapped);
		
		mif->seek(offset * 4 * 4, std::ios::beg);

		auto individual_count = 0;

		auto second_w_read = mif->read_4_bytes();

		while (second_w_read == second_w_mapped)
		{
			const auto first_w_read = mif->read_4_bytes();
			const auto third_w_read = mif->read_4_bytes();
			auto count_read = mif->read_4_bytes();

			switch (arg_count)
			{
			case 1:
				individual_count += count_read;
				break;
			case 2:
				if (first_w_mapped == first_w_read)
					variant_map[count_read].emplace_back(T(first_w_mapped, second_w_read, 0));
				break;
			case 3:
				if (first_w_mapped == first_w_read && third_w_mapped == third_w_read)
					variant_map[count_read].emplace_back(T(first_w_mapped, second_w_read, third_w_mapped));
				break;
			default:
				throw;
			}

			second_w_read = mif->read_4_bytes();
		}

		if (arg_count == 1)
			variant_map[individual_count].emplace_back(T(second_w_mapped, 0, 0));
	}

	/**
	 * Searches for the most common individual word variant
	 *
	 * @param first_w The word in question - to have diacritics added to it
	 * @return The most probable variant of the word in question
	 */
	const std::wstring& most_common(std::wstring& first_w)
	{
		PROFILE_FUNCTION();

		auto first_w_variants = get_variants(wm_, first_w);

		std::map<int, std::vector<word>> variant_map;

		for (auto first_w_variant : first_w_variants)
		{
			const auto first_w_mapped = wm_.word_to_int(first_w_variant);

			if (first_w_mapped == 0)
				continue;

			variant_map[0].emplace_back(first_w_mapped);

			search_model_for(variant_map, first_w_mapped);
		}

		if (variant_map.empty())
		{
			std::lock_guard<std::mutex> lock(foreign_words_mutex);
			s_.potentially_foreign_words_.emplace(first_w);
			return first_w;
		}
		
		if (opt_.conflict_)
			return *wm_.int_to_word(handle_conflict(variant_map, std::vector<std::wstring> {s_.first_w_with_format_, s_.second_w_with_format_, s_.third_w_with_format_}).second.first_w);
		return *wm_.int_to_word(variant_map.crbegin()->second.begin()->first_w);
	}

	/**
	 * Searches for the most common two word variant
	 *
	 *@param first_w The word directly preceding the word in question
	 * @param second_w The word in question - to have diacritics added to it
	 * @return The most probable variant of both words and their count in the model
	 */
	word_tuple_count_pair most_common_tuple(std::wstring& first_w, std::wstring& second_w)
	{
		PROFILE_FUNCTION();

		auto first_w_variants = get_variants(wm_, first_w);
		auto second_w_variants = get_variants(wm_, second_w);

		std::map<int, std::vector<word_tuple>> variant_map;

		for (auto second_w_variant : second_w_variants)
		{
			const auto second_w_mapped = wm_.word_to_int(second_w_variant);

			if (second_w_mapped == 0)
				continue;

			for (auto first_w_variant : first_w_variants)
			{
				const auto first_w_mapped = wm_.word_to_int(first_w_variant);

				if (first_w_mapped == 0)
					continue;

				search_model_for(variant_map, second_w_mapped, first_w_mapped);
			}
		}

		if (variant_map.empty())
		{
			return {&most_common(first_w), &most_common(second_w), 0};
		}

		if (opt_.conflict_)
		{
			const auto best_match = handle_conflict(variant_map, std::vector<std::wstring> {s_.first_w_with_format_, s_.second_w_with_format_, s_.third_w_with_format_});

			return {
				wm_.int_to_word(best_match.second.first_w),
				wm_.int_to_word(best_match.second.second_w),
				best_match.first
			};
		}
		return {
			wm_.int_to_word(variant_map.crbegin()->second.begin()->first_w),
			wm_.int_to_word(variant_map.crbegin()->second.begin()->second_w),
			variant_map.crbegin()->first
		};
	}

	/**
	 * Searches for the most common three word variant
	 *
	 * @param first_w The word directly preceding the word in question
	 * @param second_w The word in question - to have diacritics added to it
	 * @param third_w The word directly following the word in question

	 * @return The most probable variant of the word in question
	 */
	const std::wstring* most_common_triplet(std::wstring& first_w, std::wstring& second_w, std::wstring& third_w)
	{
		PROFILE_FUNCTION();

		const auto can_have_diacritic = check_diacritic(second_w);

		if (can_have_diacritic)
		{
			auto first_w_variants = get_variants(wm_, first_w);
			auto second_w_variants = get_variants(wm_, second_w);
			auto third_w_variants = get_variants(wm_, third_w);

			std::map<int, std::vector<word_triplet>> variant_map;

			for (auto second_w_variant : second_w_variants)
			{
				const auto second_w_mapped = wm_.word_to_int(second_w_variant);

				if (second_w_mapped == 0)
					continue;

				for (auto first_w_variant : first_w_variants)
				{
					const auto first_w_mapped = wm_.word_to_int(first_w_variant);

					if (first_w_mapped == 0)
						continue;

					for (auto third_w_variant : third_w_variants)
					{
						const auto third_w_mapped = wm_.word_to_int(third_w_variant);

						if (third_w_mapped == 0)
							continue;

						search_model_for(variant_map, second_w_mapped, first_w_mapped, third_w_mapped);
					}
				}
			}

			if (variant_map.empty())
			{
				// RETURN VALUE -->	| FIRST_WORD | SECOND_WORD | COUNT |
				const auto first_two_words = most_common_tuple(first_w, second_w);

				// RETURN VALUE --> | SECOND_WORD | THIRD_WORD | COUNT |
				const auto second_two_words = most_common_tuple(second_w, third_w);

				if (first_two_words.count < second_two_words.count)
				{
					return first_two_words.second_w;
				}
				return second_two_words.first_w;
			}

			if (opt_.conflict_)
				return wm_.int_to_word(handle_conflict(variant_map, std::vector<std::wstring> {s_.first_w_with_format_, s_.second_w_with_format_, s_.third_w_with_format_}).second.second_w);
			return wm_.int_to_word(variant_map.crbegin()->second.begin()->second_w);
		}

		return &second_w;
	}

	/**
	 * Asynchronous procedure for one triplet iteration\n
	 * Gets the most common variant of second_w, applies previous formatting to it and inserts it into a shared container of output words
	 */
	void fill_result_word(std::wstring first_w, std::wstring second_w,
	                      std::wstring third_w, int triplet_order_number, const std::wstring& second_w_with_format)
	{
		PROFILE_FUNCTION();

		if (is_formatting_string(second_w))
			return;

		prepare_words(word_wrapper{&first_w, &second_w, &third_w});

		const auto result_word = most_common_triplet(first_w, second_w, third_w);

		auto result_word_with_format = apply_previous_formatting(second_w_with_format, result_word);

		const auto return_val = std::pair<int, std::wstring>(triplet_order_number, result_word_with_format);

		std::lock_guard<std::mutex> lock(result_words_mutex);

		s_.result_words_.insert(return_val);
	}

	/**
	 * Reads the stream and collects all sequences of whitespace characters into a container
	 */
	void get_file_formatting(std::wistream& wif)
	{
		PROFILE_FUNCTION();

		wchar_t c;
		auto i = 0;
		std::wstring format;
		while (wif.get(c))
		{
			if (isspace(c))
				format.push_back(c);
			else if (!format.empty())
			{
				s_.result_formats_.insert(std::pair<int, const std::wstring>(i, format));
				format.clear();
				i++;
			}
		}
	}

	/**
	 * Dumps the contains of result_words_ and result_formats_ into the stream
	 */
	void print_result(std::wofstream& wof)
	{
		PROFILE_FUNCTION();

		for (auto i = 0; i < s_.result_words_.size(); i++)
		{
			wof << s_.result_words_[i] << s_.result_formats_[i];
		}
	}

	/**
	 * Processes current triplet\n
	 * Calls asynchronous diacritic adding procedure for second_w and shifts the last two words of the triplet forward
	 */
	void do_triplet_iteration(std::wstring& first_w, std::wstring& second_w, std::wstring& third_w,
	                          int triplet_order_number)
	{
		PROFILE_FUNCTION();

		s_.word_futures_.emplace_back(std::async(std::launch::async,
		                                      &text_processor::fill_result_word, this, first_w, second_w, third_w,
		                                      triplet_order_number, s_.second_w_with_format_));

		first_w = second_w;
		second_w = third_w;

		s_.first_w_with_format_ = s_.second_w_with_format_;
		s_.second_w_with_format_ = s_.third_w_with_format_;
	}

	/**
	 * Reads the stream and processes every word triplet it encounters\n
	 * Dumps the result into an output file based on the input stream file name
	 */
	void process_text(std::wistream& wif)
	{
		PROFILE_FUNCTION();

		std::wofstream wof;

		try
		{
			auto file_name = dynamic_cast<dia::wifstream&>(wif).get_file_name();
			wof = std::wofstream(file_name + ".out", std::ios::binary);
		}
		catch (const std::bad_cast& e)
		{
#if STDIO_EXPERIMENTAL
			wof = std::wofstream("diacstd.out", std::ios::binary);
#else
			throw_error(errors::input_file_error);
#endif
		}

		if (!wof)
			throw_error(errors::output_file_error);

		auto mif = std::ifstream(model_name, std::ios::binary);

		if (!mif)
			throw_error(errors::model_error);

		mif.close();

		std::wstring first_w, second_w, third_w;

		auto triplet_order_number = 0;

		wif >> first_w >> second_w;
		auto word_count = 2;

		s_.first_w_with_format_ = first_w;
		s_.second_w_with_format_ = second_w;

		prepare_words(word_wrapper{&first_w, &second_w});
		auto first_w_with_diacritics = most_common_tuple(first_w, second_w).first_w;

		s_.result_words_.insert(std::pair<int, std::wstring>(triplet_order_number,
		                                                  apply_previous_formatting(
			                                                  s_.first_w_with_format_, first_w_with_diacritics)));
		triplet_order_number++;

		while (wif >> third_w)
		{
			s_.third_w_with_format_ = third_w;
			s_.carry_over_word_ = separate_punctuation(third_w);

			word_count++;

			do_triplet_iteration(first_w, second_w, third_w, triplet_order_number);
			triplet_order_number++;

			if (!s_.carry_over_word_.empty())
			{
				third_w = s_.carry_over_word_;

				do_triplet_iteration(first_w, second_w, third_w, triplet_order_number);

				s_.carry_over_word_.clear();
			}
		}

		wif.clear();
		wif.seekg(0, std::ios_base::beg);

		get_file_formatting(wif);

		s_.word_futures_.clear();

#ifdef DEBUG
		if (result_words_.size() - 1 != total_words_read)
		{
			std::wcerr << total_words_read - result_words_.size() + 1 << L" words have been lost due to multi threading!\n";
			throw_error(multithreading_error);
		}
#endif

		if (!opt_.silence_ && s_.potentially_foreign_words_.size() / static_cast<double>(
			word_count) >= 0.25)
			std::wcerr << 100 * s_.potentially_foreign_words_.size() / static_cast<double>(word_count)
				<< L"% of all words have not been found in the dictionary.\n"
				<< L"It is possible that the file is not written in Czech!";

		prepare_words(word_wrapper{&first_w, &second_w});
		auto third_w_with_diacritics = most_common_tuple(first_w, second_w).second_w;

		s_.result_words_.insert(std::pair<int, std::wstring>(triplet_order_number,
		                                                  apply_previous_formatting(
			                                                  s_.second_w_with_format_, third_w_with_diacritics)));

		print_result(wof);

		s_.result_words_.clear();
		s_.result_formats_.clear();
		wof.close();
	}
};

/**
 * @return Size of the file in bytes
 */
unsigned long file_size(const std::string& filename)
{
	const auto p_file = fopen(filename.c_str(), "rb");
	if (!p_file)
		throw_error(errors::input_file_error);

	fseek(p_file, 0, SEEK_END);
	const unsigned long size = ftell(p_file);
	fclose(p_file);
	return size;
}

/**
 * Decompresses a file using zlib
 *
 * @param infilename Path to the compressed file
 * @param outfilename Path, to which the decompressed file will be dumped
 */
void decompress_one_file(const std::string& infilename, const std::string& outfilename)
{
	const auto infile = gzopen(infilename.c_str(), "rb");

	/// FILE API due to zlib requiring it
	const auto outfile = fopen(outfilename.c_str(), "wb");
	if (!infile)
		throw_error(errors::input_file_error);
	if (!outfile)
		throw_error(errors::output_file_error);

	char buffer[128];
	int num_read;
	while ((num_read = gzread(infile, buffer, sizeof buffer)) > 0)
	{
		fwrite(buffer, 1, num_read, outfile);
	}

	gzclose(infile);
	fclose(outfile);
}

/**
 * Compresses a file using zlib
 *
 * @param infilename Path to the original file
 * @param outfilename Path, to which the compressed file will be dumped
 */
void compress_one_file(const std::string& infilename, const std::string& outfilename)
{
	/// FILE API due to zlib requiring it
	const auto infile = fopen(infilename.c_str(), "rb");
	const auto outfile = gzopen(outfilename.c_str(), "wb");
	if (!infile)
		throw_error(errors::input_file_error);
	if (!outfile)
		throw_error(errors::output_file_error);

	char inbuffer[128];
	int num_read;
	unsigned long total_read = 0;
	while ((num_read = fread(inbuffer, 1, sizeof inbuffer, infile)) > 0)
	{
		total_read += num_read;
		gzwrite(outfile, inbuffer, num_read);
	}
	fclose(infile);
	gzclose(outfile);

	std::wcerr << L"Read " << total_read << L" bytes, Wrote " << file_size(outfilename) <<
		L" bytes, Compression factor "
		<< (1.0 - file_size(outfilename) * 1.0 / total_read) * 100.0 << L"%\n";
}

// ASSUMES UTF-8 
int main(int argc, char** argv)
{
#if PROFILING
	instrumentor::get().begin_session("Profile");
#endif

	SetConsoleOutputCP(65001);
	(void)std::ios_base::sync_with_stdio(false);
	(void)std::wcout.imbue(std::locale(std::locale::empty(), new std::codecvt_utf8<wchar_t>));
	(void)std::wcerr.imbue(std::locale(std::locale::empty(), new std::codecvt_utf8<wchar_t>));
	(void)std::locale::global(std::locale(std::locale::empty(), new std::codecvt_utf8<wchar_t>));

	auto conflict = false;
	auto silence = false;
	auto memory_map = false;
	auto file_less = true;

	if (argc >= 2)
	{
		if (strcmp(argv[1], "--help") == 0)
		{
			assert(argc == 2);

			std::wcerr << L"*** Diac - a tool for diacritics ***\n"
				<< L"\n(Note that the '-m' option does not yield any notable performance improvements and should only be used on systems with HDD)\n"
				<< L"\tUsage:\t 'diac -i' for installation.\n"
				<< L"\t\t'diac -[scm] [filename]' for silent, conflict resolving or memory mapping modes.\n"
				<< L"\t\t'diac -[hc] [filename]' for Huffman compression of said file.\n"
				<< L"\t\t'diac -[hd] [filename]' for Huffman decompression of said file.\n\n";

			return 0;
		}
		if (strcmp(argv[1], "-d") == 0 ||
			strcmp(argv[1], "--demo") == 0)
		{
			assert(argc == 2);

			auto opt = user_options(false, false);
			auto tp = text_processor(opt);

			std::wcerr << L"Demo:\n";

			for (auto i = 1; i <= 5; i++)
			{
				auto wif = dia::wifstream("demo0" + std::to_string(i) + ".txt");

				if (wif)
				{
					std::wcerr << L"Running demo no. " << i << " out of " << 5 << "\n";

					tp.process_text(wif);

					auto word_count = 0;
					std::vector<std::pair<std::wstring, std::wstring>> diff_words;
					auto reference_wif = dia::wifstream("demo0" + std::to_string(i) + "_ref.txt");
					auto output_wif = dia::wifstream("demo0" + std::to_string(i) + ".txt.out");

					auto diff_count = diff(reference_wif, output_wif, word_count, diff_words);

					std::wcerr << L"\tFile:\tdemo0" << i << ".txt\n"
						<< L"\t\tTotal length:\t" << word_count << " words\n"
						<< L"\t\tDifferences:\t" << diff_count << " words\n"
						<< L"\t\tAccuracy:\t" << 100 * (static_cast<double>(word_count) - diff_count) / static_cast<
							double>(word_count) << "%\n";

					if (diff_count > 0)
					{
						std::wcerr << L"\t\tList of differing words:\n";

						for (auto&& pair : diff_words)
						{
							std::wcerr << L"\t\t\t" << pair.first << L"\t" << pair.second << L"\n";
						}
					}
				}
				else
					throw_error(errors::input_file_error);

				wif.close();
			}

#if PROFILING
			instrumentor::get().end_session();
#endif

			return 0;
		}
		if (strcmp(argv[1], "-i") == 0 ||
			strcmp(argv[1], "--install") == 0)
		{
			assert(argc == 2);

			std::wcerr << L"Decompressing...\n";

			decompress_one_file("_diac_model.hzip", model_name);

			std::wcerr << L"Installation Successful!\n";

			return 0;
		}
		if (strcmp(argv[1], "-hc") == 0 ||
			strcmp(argv[1], "--compress") == 0)
		{
			assert(argc == 3);

			std::string in = argv[argc - 1];

			std::wcerr << L"Compressing...\n";

			compress_one_file(in, in + ".hzip");

			std::wcerr << L"File " << argv[argc - 1] << L" compressed successfully!\n"
				<< L"Output:\t" << argv[argc - 1] << L".hzip\n";

			return 0;
		}
		if (strcmp(argv[1], "-hd") == 0 ||
			strcmp(argv[1], "--decompress") == 0)
		{
			assert(argc == 3);

			std::string in = argv[argc - 1];

			std::wcerr << L"Decompressing...\n";

			decompress_one_file(in, in + ".out");

			std::wcerr << L"File " << argv[argc - 1] << L" decompressed successfully!\n"
				<< L"Output:\t" << argv[argc - 1] << L".out\n";

			return 0;
		}

		if (strcmp(argv[1], "-c") == 0 ||
			strcmp(argv[1], "--conflict") == 0)
		{
			conflict = true;

			if (argc >= 3)
				file_less = false;

			if (strcmp(argv[2], "-s") == 0 ||
				strcmp(argv[2], "--silent") == 0)
			{
				silence = true;
				if (argc == 4)
					file_less = false;
			}
			else if (strcmp(argv[2], "-m") == 0 ||
				strcmp(argv[2], "--memory") == 0)
			{
				memory_map = true;
				if (argc == 4)
					file_less = false;
			}
			
			if (strcmp(argv[3], "-s") == 0 ||
				strcmp(argv[3], "--silent") == 0)
			{
				silence = true;
				if (argc == 5)
					file_less = false;
			}
			else if (strcmp(argv[3], "-m") == 0 ||
				strcmp(argv[3], "--memory") == 0)
			{
				memory_map = true;
				if (argc == 5)
					file_less = false;
			}
		}
		else if (strcmp(argv[1], "-s") == 0 ||
			strcmp(argv[1], "--silent") == 0)
		{
			silence = true;

			if (argc >= 3)
				file_less = false;

			if (strcmp(argv[2], "-c") == 0 ||
				strcmp(argv[2], "--conflict") == 0)
			{
				conflict = true;
				if (argc == 4)
					file_less = false;
			}
			else if (strcmp(argv[2], "-m") == 0 ||
				strcmp(argv[2], "--memory") == 0)
			{
				memory_map = true;
				if (argc == 4)
					file_less = false;
			}

			if (strcmp(argv[3], "-c") == 0 ||
				strcmp(argv[3], "--conflict") == 0)
			{
				conflict = true;
				if (argc == 5)
					file_less = false;
			}
			else if (strcmp(argv[3], "-m") == 0 ||
				strcmp(argv[3], "--memory") == 0)
			{
				memory_map = true;
				if (argc == 5)
					file_less = false;
			}
		}
		else if (strcmp(argv[1], "-cs") == 0 ||
			strcmp(argv[1], "-sc") == 0)
		{
			conflict = true;
			silence = true;

			if (argc == 3)
				file_less = false;
		}
		else if (strcmp(argv[1], "-m") == 0 ||
			strcmp(argv[1], "--memory") == 0)
		{
			memory_map = true;

			if (argc >= 3)
				file_less = false;

			if (strcmp(argv[2], "-c") == 0 ||
				strcmp(argv[2], "--conflict") == 0)
			{
				conflict = true;
				if (argc == 4)
					file_less = false;
			}
			else if (strcmp(argv[2], "-s") == 0 ||
				strcmp(argv[2], "--silent") == 0)
			{
				silence = true;
				if (argc == 4)
					file_less = false;
			}

			if (strcmp(argv[3], "-c") == 0 ||
				strcmp(argv[3], "--conflict") == 0)
			{
				conflict = true;
				if (argc == 5)
					file_less = false;
			}
			else if (strcmp(argv[3], "-s") == 0 ||
				strcmp(argv[3], "--silent") == 0)
			{
				silence = true;
				if (argc == 5)
					file_less = false;
			}
		}
		else if (strcmp(argv[1], "-mcs") == 0 ||
			strcmp(argv[1], "-cms") == 0 ||
			strcmp(argv[1], "-csm") == 0 || 
			strcmp(argv[1], "-scm") == 0 || 
			strcmp(argv[1], "-msc") == 0)
		{
			conflict = true;
			silence = true;
			memory_map = true;
			
			if (argc == 3)
				file_less = false;
		}
		else
			file_less = false;
	}


	auto opt = user_options(silence, conflict);
	auto tp = text_processor(opt);

	if (file_less)
	{
		/// STDIN TO STDOUT MODE
#if STDIO_EXPERIMENTAL
		tp.process_text(std::wcin);
#else
		throw_error(errors::input_file_error);
#endif
	}
	else
	{
		/// FILE TO FILE MODE
		auto wif = dia::wifstream(argv[argc - 1]);

		if (wif)
			tp.process_text(wif);
		else
			throw_error(errors::input_file_error);

		wif.close();
	}

#if PROFILING
	instrumentor::get().end_session();
#endif

	return 0;
}
