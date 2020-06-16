#pragma once
#include <cassert>
#include <iostream>
#include <map>
#include <vector>

/**
 * Finds the frontier for the most reasonable word variants
 *
 * @param variant_map Variant map of either word, word_tuple or word_triplet
 * @return An iterator pointing to the least still satisfactory word variant
 */
template <typename T>
auto get_reasonable_lower_bound(std::map<int, std::vector<T>>& variant_map)
{
	const auto max_count_candidates = 4;

	size_t average_count = 0;
	size_t total_count = 0;

	for (auto it = variant_map.cbegin(); it != variant_map.cend(); ++it)
	{
		average_count += it->first * it->second.size();
		total_count += it->second.size();
	}

	average_count /= total_count;

	auto i = 1;

	for (auto it = variant_map.crbegin(); it != variant_map.crend(); ++it)
	{
		if (it->first > average_count&& i < max_count_candidates)
		{
			i++;
			continue;
		}

		return it;
	}

	return variant_map.crbegin();
}

/**
 * Prints a list of satisfactory word variants and lets the user choose via console input
 * @param variant_map Variant map of either word, word_tuple or word_triplet
 * @param context The three words from the currently processed triplet
 * @return The user choice and its count
 */
template <typename T>
auto handle_conflict(std::map<int, std::vector<T>>& variant_map, const std::vector<std::wstring>& context)
{
	auto lower_bound = get_reasonable_lower_bound(variant_map);

	if (lower_bound == variant_map.crbegin() && variant_map.crbegin()->second.size() == 1)
	{
		return std::pair<int, T>(variant_map.crbegin()->first, *variant_map.crbegin()->second.begin());
	}

	assert(context.size() == 3);

	std::wcerr
		<< L"A conflict has been found:\n"
		<< context[0] << L" " << context[1] << L" " << context[2] << L"\n"
		<< L"Select the correct option below:\n";

	auto i = 1;

	for (auto it = variant_map.crbegin(); it != lower_bound; ++it)
	{
		for (auto&& vec_it : it->second)
		{
			std::wcerr << i << L")\t" << static_cast<int>(vec_it) << L"\n";
			i++;
		}
	}

	auto user_choice = 0;
	std::cin >> user_choice;

	i = 1;

	for (auto it = variant_map.crbegin(); it != lower_bound; ++it)
	{
		for (auto&& vec_it : it->second)
		{
			if (user_choice == i)
				return std::pair<int, T>(it->first, vec_it);

			i++;
		}
	}

	throw;
}