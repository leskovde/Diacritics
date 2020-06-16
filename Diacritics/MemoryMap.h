#pragma once
#include <memory>
#include <cassert>
#include <string>
#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <atomic>

#include "ErrorHandler.h"
#include <fstream>
#include <unordered_map>

#define PAGE_SIZE (4096 * 1024)

inline bool is_power_of_two(const size_t n)
{
	return n != 0 && (n & n - 1) == 0;
}

/**
 * An object that stores N bytes of data from a file, preferably 4096 KB, and an offset of the page in said file in bytes
 */
template <size_t N>
class page
{
	const size_t page_size_ = N;
	const size_t offset_from_beg_;
	std::shared_ptr<char[]> contains_;

public:
	page(const size_t offset, const std::string& file) : offset_from_beg_(offset)
	{
		PROFILE_FUNCTION();
		
		assert(is_power_of_two(page_size_) == true);
		contains_ = std::shared_ptr<char[]>(new char[page_size_]);

		std::ifstream ifs(file, std::ios::in | std::ios::binary);

		if(ifs)
		{
			ifs.seekg(offset_from_beg_, std::ios::_Seekbeg);
			ifs.read(contains_.get(), page_size_);
		}
	}

	/**
	 * Returns a pointer to the segment of memory that stores all the data
	 */
	[[nodiscard]] std::shared_ptr<char[]> get_data_handle() const
	{
		return contains_;
	}

	[[nodiscard]] size_t size() const
	{
		return page_size_;
	}

	[[nodiscard]] size_t get_offset() const
	{
		return offset_from_beg_;
	}
};

/**
 * A collection of pages that represents a single file.\n
 * Pages are allocated continually as different segments of the file are read.
 */
class mem_map
{
	const size_t file_size_;
	const std::string file_name_;
	const static size_t page_size = PAGE_SIZE;
	std::unordered_map<size_t, page<page_size>> page_map_;
	mutable std::shared_mutex page_mutex_;

public:
	explicit mem_map(const std::string& file) : file_size_(std::filesystem::file_size(file)), file_name_(file)
	{
		std::wcout << L"The memory caching option has been used. Note that the supporting files will be slowly loaded into the system's memory.\n"
			<< L"This option might perform much worse than the standard configuration. This is especially true for systems with fast storage.\n";
	}

	/**
	 * Supporting and optional function that loads the entire file at once, essentially defeating the purpose of this class.\n
	 * Yields notable performance improvements compared to the normal version.
	 */
	void load_all_pages()
	{
		// lock mutex
		std::unique_lock write_lock(page_mutex_);

		for (auto i = 0; i < file_size_; i += page_size)
		{
			// create UNIQUE page
			auto page_ = page<page_size>(i, file_name_);
			// touch the page_map_
			page_map_.emplace(i, std::move(page_));
		}
		// unlock mutex
	}

	/**
	 * Reads 'count' of bytes from 'offset' position in file to the 'buffer' storage.
	 */
	void read(char* buffer, const size_t count, const size_t offset)
	{
		PROFILE_FUNCTION();
		
		size_t nearest_offset = (offset / page_size) * page_size;

		// lock mutex for reading
		page_mutex_.lock_shared();
		auto it = page_map_.find(nearest_offset);

		// unlock mutex for reading
		page_mutex_.unlock_shared();

		if (it == page_map_.end())
		{
			// create a page - each page could have a dedicated mutex to prevent loading the same page multiple times
			auto page_ = page<page_size>(nearest_offset, file_name_);

			// lock mutex for writing
			std::unique_lock write_lock(page_mutex_);
			it = page_map_.emplace(nearest_offset, std::move(page_)).first;

			// unlock mutex for writing
		}

		for (auto i = 0; i < count; i++)
		{
			if (offset - nearest_offset + i >= page_size)
			{
				nearest_offset += page_size;

				// lock mutex for reading
				page_mutex_.lock_shared();
				it = page_map_.find(nearest_offset);
				
				// unlock mutex for reading
				page_mutex_.unlock_shared();

				if (it == page_map_.end())
				{
					// create a page - each page could have a dedicated mutex to prevent loading the same page multiple times
					auto page_ = page<page_size>(nearest_offset, file_name_);

					// lock mutex for writing
					std::unique_lock write_lock(page_mutex_);
					it = page_map_.emplace(nearest_offset, std::move(page_)).first;

					// unlock mutex for writing
				}
			}

			buffer[i] = it->second.get_data_handle()[offset - nearest_offset + i];
		}
	}

	size_t size() const
	{
		return file_size_;
	}
};