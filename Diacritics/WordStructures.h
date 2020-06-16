#pragma once
/**
 * Stores a single word in int form
 */
struct word
{
	int first_w;

	explicit word(const int first, const int second = 0, const int third = 0)
	{
		this->first_w = first;
	}

	bool operator<(const word& w) const
	{
		return first_w < w.first_w;
	}

	explicit operator int() const { return first_w; }
};

/**
 * Stores two words in int form
 */
struct word_tuple
{
	int first_w, second_w;

	word_tuple(const int first, const int second, const int third = 0)
	{
		this->first_w = first;
		this->second_w = second;
	}

	bool operator<(const word_tuple& wt) const
	{
		if (first_w == wt.first_w)
		{
			return second_w < wt.second_w;
		}
		return first_w < wt.first_w;
	}

	explicit operator int() const { return second_w; }
};

/**
 * Stores three word in int form
 */
struct word_triplet
{
	int first_w, second_w, third_w;

	word_triplet(const int first, const int second, const int third)
	{
		this->first_w = first;
		this->second_w = second;
		this->third_w = third;
	}

	bool operator<(const word_triplet& wt) const
	{
		if (first_w == wt.first_w)
		{
			if (second_w == wt.second_w)
			{
				return third_w < wt.third_w;
			}

			return second_w < wt.second_w;
		}
		return first_w < wt.first_w;
	}

	explicit operator int() const { return second_w; }
};

/**
 * Stores a single word in std::wstring* format and a frequency count
 */
struct word_count_pair
{
	int count;
	const std::wstring* word;

	word_count_pair(const std::wstring* word, const int count)
	{
		this->count = count;
		this->word = word;
	}
};

/**
 * Stores two words in std::wstring* format and a frequency count
 */
struct word_tuple_count_pair
{
	int count;
	const std::wstring* first_w;
	const std::wstring* second_w;

	word_tuple_count_pair(const std::wstring* first_w, const int count)
	{
		this->count = count;
		this->first_w = first_w;
		this->second_w = nullptr;
	}

	word_tuple_count_pair(const std::wstring* first_w, const std::wstring* second_w, const int count)
	{
		this->count = count;
		this->first_w = first_w;
		this->second_w = second_w;
	}
};
