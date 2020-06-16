#pragma once
#include <iosfwd>
#include <cstdint>
#include <string>
#include <fstream>
#include "MemoryMap.h"

extern const char* model_name;

class binary_reader
{
public:

	virtual ~binary_reader() = default;
	binary_reader() = default;
	binary_reader(binary_reader&&) = default;
	binary_reader(const binary_reader&) = default;
	binary_reader& operator=(const binary_reader&) = default;
	binary_reader& operator=(binary_reader&&) = default;

	virtual int32_t read_4_bytes() = 0;
	virtual void seek(long long offset, std::ios::_Seekdir direction) = 0;
};

/**
 * A binary reader that uses file streams and STL methods.
 */
class ifstream_binary_reader final : public binary_reader
{
	std::ifstream ifs_;

public:

	explicit ifstream_binary_reader(const std::string& filename) : ifs_(std::ifstream(model_name, std::ios::binary)) { }

	int32_t read_4_bytes() override
	{	
		int32_t value;
		ifs_.read(reinterpret_cast<char*>(&value), sizeof value);

		return value;
	}

	void seek(const long long offset, const std::ios::_Seekdir direction) override
	{
		
		ifs_.seekg(offset, direction);
	}
};

/**
 * A 'memory mapped' binary reader that uses the process memory to cache previously read segments.
 */
class mmap_binary_reader final : public binary_reader
{
	mem_map& mm_;
	size_t offset_ = 0;
	
public:

	explicit mmap_binary_reader(mem_map& mm) : mm_(mm)
	{
	}

	int32_t read_4_bytes() override
	{	
		int32_t value;
		mm_.read(reinterpret_cast<char*>(&value), sizeof value, offset_);
		offset_ += sizeof value;

		return value;
	}

	void seek(const long long offset, const std::ios::_Seekdir direction) override
	{	
		switch (direction)
		{
			case std::ios::_Seekbeg:
				offset_ = offset;
				break;

			case std::ios::_Seekcur:
				offset_ += offset;
				break;
			
			case std::ios::_Seekend:
				offset_ = mm_.size() + offset;
				break; 

			default:
				throw_error(errors::model_error);
		}
	}
};