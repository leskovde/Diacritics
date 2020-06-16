#include "WideCharUtilities.h"
#include <string>
#include <set>
#include <algorithm>
#include <iostream>

static std::wstring lowercase = L"áčďéěíňóřšťúůýž";
static std::wstring uppercase = L"ÁČĎÉĚÍŇÓŘŠŤÚŮÝŽ";
static std::wstring diacritic_letters = L"áčďéěíňóřšťúůýžÁČĎÉĚÍŇÓŘŠŤÚŮÝŽ";
static std::wstring non_diacritics = L"acdeeinorstuuyz";
static std::wstring diacritics = L"áčďéěíňóřšťúůýž";

/**
 * Converts classic and Czech wchar_t characters to lowercase
 */
wchar_t to_lower_case(const wchar_t c)
{
	for (auto i = 0; i < uppercase.size(); i++)
	{
		if (uppercase[i] == c)
			return lowercase[i];
	}

	return tolower(c);
}

/**
 * Converts classic and Czech wchar_t characters to uppercase
 */
wchar_t to_upper_case(const wchar_t c)
{
	for (auto i = 0; i < lowercase.size(); i++)
	{
		if (lowercase[i] == c)
			return uppercase[i];
	}

	return toupper(c);
}

/**
 * @return True if a classic or Czech wchar_t is uppercase
 */
bool is_upper_case(const wchar_t c)
{
	if (c >= 'A' && c <= 'Z' || uppercase.find(c) != std::wstring::npos)
		return true;

	return false;
}

/**
 * @return True if wchar_t represents a letter with diacritics
 */
bool has_diacritics(const wchar_t c)
{
	for (auto& it : diacritic_letters)
	{
		if (it == c)
			return true;
	}

	return false;
}

/**
 * @return True if wchar_t can have a variant with diacritics
 */
bool can_have_diacritics(const wchar_t c)
{
	if (c >= L'a' && c <= L'z' || c >= L'A' && c <= L'Z')
		return true;

	return false;
}

/**
 * @return A string of diacritic variants of wchar_t
 */
std::wstring get_letter_diacritics(const wchar_t c)
{
	std::wstring letter_variants;

	for (auto i = 0; i < non_diacritics.size(); i++)
	{
		if (non_diacritics[i] == c)
		{
			letter_variants.push_back(diacritics[i]);
		}
	}

	return letter_variants;
}

/**
 * Recursive function that generates diacritic variants for a given word
 */
void get_word_variants(word_mapping& wm, std::set<std::wstring>& variants, std::wstring word, const size_t start,
                       const size_t end)
{
	for (auto i = start; i <= end; i++)
	{
		if (can_have_diacritics(word[i]))
		{
			auto letter_variants = get_letter_diacritics(word[i]);

			for (auto j = 0; j < letter_variants.size(); j++)
			{
				const auto word_backtrack = word;
				word.replace(i, 1, letter_variants, j, 1);

				if (wm.word_to_int(word) != 0)
					variants.insert(word);

				get_word_variants(wm, variants, word, start + 1, end);

				word = word_backtrack;
			}
		}
	}
}

/**
 * Removes quotes, commas, periods, ... from a given word, unless that word contains only those characters
 */
void delete_formatting_characters(std::wstring& word)
{
	if (word.size() == 1)
		return;

	auto full_of_formatting_chars = true;

	for (auto c : word)
	{
		if (!is_formatting_character(c))
			full_of_formatting_chars = false;
	}

	if (full_of_formatting_chars)
		return;

	word.erase(std::remove(word.begin(), word.end(), L'\''), word.end());
	word.erase(std::remove(word.begin(), word.end(), L'\"'), word.end());
	word.erase(std::remove(word.begin(), word.end(), L'.'), word.end());
	word.erase(std::remove(word.begin(), word.end(), L','), word.end());
	word.erase(std::remove(word.begin(), word.end(), L'„'), word.end());
	word.erase(std::remove(word.begin(), word.end(), L'“'), word.end());
	word.erase(std::remove(word.begin(), word.end(), L'…'), word.end());
	word.erase(std::remove(word.begin(), word.end(), L':'), word.end());
	word.erase(std::remove(word.begin(), word.end(), L';'), word.end());
}

/**
 * Converts a list of words into lowercase and removes quotes, commas, ... from them
 */
void prepare_words(std::list<std::wstring*>&& words)
{
	for (auto word : words)
	{
		delete_formatting_characters(*word);

		std::transform(word->begin(), word->end(), word->begin(), to_lower_case);
	}
}

/**
 * Checks whether a given word can have a diacritic variant (even a invalid one)
 */
bool check_diacritic(std::wstring& word)
{
	auto can_have_diacritic = false;

	for (auto&& letter : word)
	{
		if (can_have_diacritics(letter))
		{
			can_have_diacritic = true;
			break;
		}
	}

	return can_have_diacritic;
}

/**
 * @return A set of valid (present in the dictionary) variants of a word
 */
auto get_variants(word_mapping& wm, std::wstring& first_w) -> std::set<std::wstring>
{
	std::set<std::wstring> first_w_variants;
	get_word_variants(wm, first_w_variants, first_w, 0, first_w.size() - 1);
	first_w_variants.insert(first_w);

	return first_w_variants;
}

/**
 * Strips punctuation from the word passed via arguments and returns it as the return value\n
 * The original word is modified!
 */
std::wstring separate_punctuation(std::wstring& word)
{
	std::wstring punctuation;

	if (word.size() > 1)
	{
		for (auto it = word.rbegin(); it != word.rend(); ++it)
		{
			if (*it == L'.' || *it == L',' || *it == L'?' || *it == L'!')
				punctuation.push_back(*it);
			else
				break;
		}

		for (auto i = 0; i < punctuation.size(); i++)
			word.pop_back();
	}
	return punctuation;
}

/**
 * Compares two files and returns the number of differing characters
 */
int char_diff(std::wifstream& original, std::wifstream& changed, int& length)
{
	auto differences = 0;
	length = 0;

	wchar_t file1_char, file2_char;

	while (original.get(file1_char) && changed.get(file2_char))
	{
		length++;

		if (file1_char != file2_char)
			differences++;
	}

	return differences;
}

/**
 * Compares two files and returns the number of differing words as well as a container of differing words
 */
int diff(std::wifstream& original, std::wifstream& changed, int& word_count,
         std::vector<std::pair<std::wstring, std::wstring>>& diff_words)
{
	auto differences = 0;
	word_count = 0;

	std::wstring file1_word, file2_word;

	while (original >> file1_word)
	{
		word_count++;

		if (changed >> file2_word)
		{
			if (file1_word != file2_word)
			{
				differences++;
				diff_words.emplace_back(file1_word, file2_word);
			}
		}
		else
		{
			differences++;
			diff_words.emplace_back(file1_word, L"-Missing Word-");
		}
	}

	return differences;
}

bool is_formatting_character(const wchar_t c)
{
	if (c == L'\"' || c == L'\'' || c == L'„' || c == L'“' || c == L'…' || c == L'.' || c == L',' || c == L'?' || c ==
		L'!' || c == L':' || c == L';')
		return true;

	return false;
}

bool is_formatting_string(const std::wstring& s)
{
	if (s == L"," || s == L"." || s == L"?" || s == L"!" || s == L"..." || s == L":" || s == L";")
		return true;

	return false;
}

/**
 * Adds formatting such as uppercase letters, quotes, commas and periods to the unformatted word based on the reference word
 *
 * @param reference_word Word with formatting characters (quotes, commas, ...)
 * @param unformatted_word Format-lees variant of reference_word
 * @return unformatted_word with the format from the reference_word
 */
std::wstring apply_previous_formatting(const std::remove_reference<std::basic_string<wchar_t>&>::type&
	reference_word,
	const std::wstring* const unformatted_word)
{
	std::wstring result_word_with_format;
	auto j = 0;

	for (auto&& c : reference_word)
	{
		if (isdigit(c))
		{
			result_word_with_format = reference_word;
			break;
		}

		if (is_formatting_character(c))
		{
			result_word_with_format.push_back(c);
		}
		else if (is_upper_case(c))
		{
			result_word_with_format.push_back(to_upper_case((*unformatted_word)[j]));
			j++;
		}
		else
		{
			result_word_with_format.push_back((*unformatted_word)[j]);
			j++;
		}
	}

	return result_word_with_format;
}
