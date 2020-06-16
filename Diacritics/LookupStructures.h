#pragma once
#include <utility>
#include <unordered_map>
#include <fstream>

/**
 * A mutable unordered bimap for std::wstring and int values
 */
class mutable_word_mapping
{
public:
	std::unordered_map<std::wstring, int> word_to_int_map;
	std::unordered_map<int, const std::wstring> int_to_word_map;

	mutable_word_mapping() = default;

	void insert(std::wstring&& word, int number)
	{
		const auto it = word_to_int_map.emplace(std::pair<std::wstring, int>(word, number));
		int_to_word_map.emplace(std::pair<int, const std::wstring>(number, it.first->first));
#ifdef DEBUG
		std::wcout << L"Word To Int Map address: " << &(word_to_int_map_.cbegin()->first)
			<< L", Word To Int Map word: " << (word_to_int_map_.cbegin()->first) << L"\n";
		std::wcout << L"Int To Word Map address: " << int_to_word_map_.cbegin()->second
			<< L", Int To Word Map word: " << *(int_to_word_map_.cbegin()->second) << L"\n";
#endif
	}

	int word_to_int(std::wstring& word)
	{
		const auto return_val = word_to_int_map[word];

		if (return_val)
			return return_val;

		return 0;
	}

	const std::wstring& int_to_word(const int number)
	{
		return int_to_word_map[number];
	}
};

/**
 * An immutable unordered bimap for std::wstring and int values\n
 * Instances are created by making a copy of an existing mutable_word_mapping object
 */
class word_mapping
{
	const std::unordered_map<std::wstring, int> word_to_int_map_;
	const std::unordered_map<int, const std::wstring> int_to_word_map_;

public:
	word_mapping() = default;

	explicit word_mapping(mutable_word_mapping&& wm) : word_to_int_map_(wm.word_to_int_map),
	                                                   int_to_word_map_(wm.int_to_word_map)
	{
	}


	int word_to_int(std::wstring& word) const
	{
		const auto return_val = word_to_int_map_.find(word);

		if (return_val != word_to_int_map_.end())
			return return_val->second;

		return 0;
	}

	const std::wstring* int_to_word(const int number) const
	{
		return &int_to_word_map_.find(number)->second;
	}
};

/**
 * An immutable hashtable for words in int format and line offset values\n
 */
class mutable_offset_table
{
public:
	std::unordered_map<int, int> compressed_model;

	void insert(int key, int count)
	{
		compressed_model.insert(std::pair<int, int>(key, count));
	}

	int get_value(const int key)
	{
		return compressed_model[key];
	}

	size_t size() const
	{
		return compressed_model.size();
	}
};

/**
 * An immutable hashtable for words in int format and line offset values\n
 * Instances are created by making a copy of an existing mutable_model object
 */
class offset_table
{
	const std::unordered_map<int, int> compressed_model_;

public:
	offset_table() = default;

	explicit offset_table(mutable_offset_table&& m) : compressed_model_(m.compressed_model)
	{
	}

	int get_value(const int key) const
	{
		return compressed_model_.find(key)->second;
	}

	size_t size() const
	{
		return compressed_model_.size();
	}
};
