#include "DataPreparation.h"
#include <iostream>
#include <fstream>
#include <set>
#include <sstream>
#include "ErrorHandler.h"

/**
 * Merges two files into one that is stored separately, stores only unique std::wstring value
 */
void merge_dictionaries(const std::string& file1, const std::string& file2)
{
	std::wifstream wif1(file1, std::ios::binary);
	std::wifstream wif2(file2, std::ios::binary);
	std::wofstream wof("final.dic", std::ios::binary);

	if (!(wif1 && wif2 && wof))
	{
		std::cout << "File Error" << std::endl;
		return;
	}

	std::set<std::wstring> additional_words;

	std::wstring current_word;

	while (std::getline(wif1, current_word))
	{
		if (current_word.back() == L'\r')
			current_word.pop_back();

		additional_words.insert(current_word);

		wof << current_word << std::endl;
	}

	while (std::getline(wif2, current_word))
	{
		additional_words.insert(current_word);
	}

	for (auto&& element : additional_words)
	{
		wof << element << std::endl;
	}

	wif1.close();
	wif2.close();
	wof.close();
}

/**
 * Obsolete - compresses a text model into 16B per line format
 */
void compress_model_to_4_b(const std::string& filename)
{
	std::wifstream wif(filename, std::ios::binary);
	std::ofstream ofs("4b.model", std::ios::binary);

	int_least32_t word;

	while (wif >> word)
	{
		ofs.write(reinterpret_cast<const char*>(&word), sizeof word);
	}

	wif.close();
	ofs.close();
}

/**
 * Creates the offset file - traverses the model file and creates a key value pair for every word\n
 * key - word in the int representation, value - line offset from the start
 * Results are dumped into an output file, every int value is put onto a separate line (every key value pair takes up two lines)
 */
mutable_offset_table generate_compressed_model(const std::string& filename)
{
	std::wifstream wif(filename, std::ios::binary);
	std::wofstream wof("compressed.model", std::ios::binary);

	mutable_offset_table m;

	auto key_numeric = 1;
	auto count = 0;
	std::wstring current_line, key;

	while (std::getline(wif, current_line))
	{
		std::wstringstream wss(current_line);

		std::getline(wss, key, L' ');

		if (key_numeric != stoi(key))
		{
			m.insert(key_numeric, count);

			wof << key_numeric << L"\n" << count << L"\n";

			key_numeric = stoi(key);
		}
		count++;
	}

	wif.close();
	wof.close();

	return m;
}

/**
 * Obsolete - loads the entire obsolete text model into memory
 * Not used - takes up too much memory
 */
void load_trigram_model(const std::string& filename)
{
	std::unordered_map<int, std::list<int>> model;

	std::wifstream wif(filename, std::ios::binary);

	std::wstring current_line;
	std::wstring first_w, second_w, third_w, count;

	auto i = 1;
	while (std::getline(wif, current_line))
	{
		std::wstringstream wss(current_line);

		std::getline(wss, first_w, L' ');
		std::getline(wss, second_w, L' ');
		std::getline(wss, third_w, L' ');
		std::getline(wss, count, L' ');

		auto frequency = stoi(count);

		if (frequency > 1)
		{
			model[frequency].push_back(i);
		}

		i++;
	}

	wif.close();
}

/**
* Loads the contents of an offset file into a model object
*
* @param filename Path to the offset file
* @return Mutable copy of a hashtable containing int codes of words as keys and line offsets as values
*/
mutable_offset_table load_compressed_model(const std::string& filename)
{
	std::ifstream iff(filename);

	if (!iff)
		throw_error(errors::offset_model_error);

	auto mutable_m = mutable_offset_table();

	int key, count, prev_count = 0;

	while (iff >> key >> count)
	{
		mutable_m.insert(key, prev_count);
		prev_count = count;
	}

	iff.close();

	return mutable_m;
}

/**
 * Loads the contents of a dictionary into a word_mapping object
 *
 * @param filename Path to the dictionary file
 * @return Mutable copy of a bimap of the dictionary
 */
mutable_word_mapping load_word_mapping(const std::string& filename)
{
	mutable_word_mapping wm;

	auto i = 1;

	std::wifstream wif(filename, std::ios::binary);

	if (!wif)
		throw_error(errors::dictionary_error);

	std::wstring current_word;

	while (std::getline(wif, current_word))
	{
#ifdef DEBUG
		std::wcerr << L"Inserting " << current_word << L"\n";
#endif

		if (current_word.back() == '\r')
			current_word.pop_back();

		wm.insert(std::move(current_word), i);
		i++;
	}

	wif.close();

	return wm;
}
