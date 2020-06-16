#pragma once
#include "LookupStructures.h"
#include <string>
#include <set>
#include <list>
#include <fstream>
#include <vector>


wchar_t to_lower_case(wchar_t);

bool has_diacritics(wchar_t c);

bool can_have_diacritics(wchar_t c);

std::wstring get_letter_diacritics(wchar_t);

void get_word_variants(word_mapping&, std::set<std::wstring>&, std::wstring, size_t, size_t);

void delete_formatting_characters(std::wstring& word);

void prepare_words(std::list<std::wstring*>&&);

bool check_diacritic(std::wstring&);

auto get_variants(word_mapping&, std::wstring& first_w) -> std::set<std::wstring>;

std::wstring separate_punctuation(std::wstring& word);

bool is_upper_case(wchar_t c);

wchar_t to_upper_case(wchar_t);

int diff(std::wifstream&, std::wifstream&, int&, std::vector<std::pair<std::wstring, std::wstring>>&);

bool is_formatting_character(wchar_t);

bool is_formatting_string(const std::wstring&);

std::wstring apply_previous_formatting(const std::remove_reference<std::basic_string<wchar_t>&>::type&,
	const std::wstring*);