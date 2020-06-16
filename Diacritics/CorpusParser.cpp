#include "CorpusParser.h"
#include <string>
#include <iostream>
#include <algorithm>
#include "WideCharUtilities.h"
#include <sstream>
#include "WordStructures.h"
#include "LookupStructures.h"
#include <map>

/**
 * Obsolete - parses corpus in range (bytes) specified in arguments\n
 * Output is dumped to a separate file
 */
void parse_corpus_in_range(const std::string& filename, size_t beginning, size_t end)
{
	std::wifstream wif(filename, std::ios::binary);
	std::wofstream wof("syn" + std::to_string(beginning) + ".out", std::ios::binary);

	if (!(wif && wof))
	{
		std::cout << "File Error" << std::endl;
		return;
	}

	wif.seekg(beginning, std::ios::beg);

	std::wstring current_line, current_word;

	while (beginning < end)
	{
		beginning++;

		if (!std::getline(wif, current_line))
			break;

		if (current_line[0] == '<')
			continue;

		std::wstringstream wss(current_line);

		std::getline(wss, current_word, L'\t');
		std::transform(current_word.begin(), current_word.end(), current_word.begin(), to_lower_case);

		if (current_word == L".")
		{
			current_word += L'\n';
		}

		wof << current_word << L' ';
	}

	wif.close();
	wof.close();
}

/**
 * Parses any SYN20XX corpus and converts it to a sequence of space separated lowercase words\n
 * The result is dumped continually to the output stream
 */
void parse_corpus(std::wifstream& wif, std::wofstream& wof)
{
	std::wstring current_line;
	std::wstring current_word;

	while (std::getline(wif, current_line))
	{
		if (current_line[0] == '<')
			continue;

		std::wstringstream wss(current_line);

		std::getline(wss, current_word, L'\t');
		std::transform(current_word.begin(), current_word.end(), current_word.begin(), to_lower_case);

		wof << current_word << L' ';
	}
}

size_t count_lines(std::wifstream& wif)
{
	size_t line_count = 0;

	std::wstring current_line;

	while (std::getline(wif, current_line))
		line_count++;

	return line_count;
}

/**
 * Reads a space separated stream of lowercase words and creates a statistic of every triplet of words it encounters and dumps it into a file\n
 * nondiacritic flag specifies whether the output should also feature triplets with middle word that has diacritics\n
 * The output file is in a 16B per line format - first 3x 4B are INT32 word values (refer to word_mapping.int_to_word()), the last 4B value is the triplet count (INT32) 
 */
void create_trigram_model(const std::string& filename, word_mapping& wm, bool include_nondiacritic = true)
{
	std::map<word_triplet, int> model;

	std::wifstream wif(filename, std::ios::binary);
	std::ofstream off("dia_4b.model", std::ios::binary);

	std::wstring first_w, second_w, third_w;

	std::getline(wif, first_w, L' ');
	std::getline(wif, second_w, L' ');

	while (std::getline(wif, third_w, L' '))
	{
		auto has_diacritic = false;

		for (auto&& letter : second_w)
		{
			if (has_diacritics(letter))
			{
				has_diacritic = true;
				break;
			}
		}

		if (include_nondiacritic || has_diacritic)
		{
			if (first_w.back() == L'\n')
				first_w.pop_back();
			if (second_w.back() == L'\n')
				second_w.pop_back();
			if (third_w.back() == L'\n')
				third_w.pop_back();

			auto first_w_mapped = wm.word_to_int(first_w);
			auto second_w_mapped = wm.word_to_int(second_w);
			auto third_w_mapped = wm.word_to_int(third_w);

			auto key = word_triplet(first_w_mapped, second_w_mapped, third_w_mapped);

			auto it = model.find(key);

			if (it != model.end())
			{
				it->second++;
			}
			else
			{
				model.insert(std::pair<word_triplet, int>(key, 1));
			}
		}

		first_w = second_w;
		second_w = third_w;
	}

	for (auto&& pair : model)
	{
		off.write(reinterpret_cast<const char*>(&pair.first.second_w), sizeof pair.first.second_w);
		off.write(reinterpret_cast<const char*>(&pair.first.first_w), sizeof pair.first.first_w);
		off.write(reinterpret_cast<const char*>(&pair.first.third_w), sizeof pair.first.third_w);
		off.write(reinterpret_cast<const char*>(&pair.second), sizeof pair.second);
	}

	wif.close();
	off.close();
}
