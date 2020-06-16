#pragma once

#include <string>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <mutex>
#include <thread>

#define PROFILING 0
#if PROFILING
#define PROFILE_SCOPE(name) instrumentation_timer timer##__LINE__(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCSIG__)
#else
#define PROFILE_SCOPE(name)
#define PROFILE_FUNCTION()
#endif

struct profile_result
{
	std::string name;
	long long start, end;
	uint32_t thread_id;
};

struct instrumentation_session
{
	std::string name;
};

class instrumentor
{
	instrumentation_session* m_current_session_;
	std::ofstream m_output_stream_;
	int m_profile_count_;
	std::mutex m_lock_;

public:
	instrumentor()
		: m_current_session_(nullptr), m_profile_count_(0)
	{
	}

	void begin_session(const std::string& name, const std::string& filepath = "results.json")
	{
		m_output_stream_.open(filepath);
		write_header();
		m_current_session_ = new instrumentation_session{name};
	}

	void end_session()
	{
		write_footer();
		m_output_stream_.close();
		delete m_current_session_;
		m_current_session_ = nullptr;
		m_profile_count_ = 0;
	}

	void write_profile(const profile_result& result)
	{
		std::lock_guard<std::mutex> lock(m_lock_);

		if (m_profile_count_++ > 0)
			m_output_stream_ << ",";

		auto name = result.name;
		std::replace(name.begin(), name.end(), '"', '\'');

		m_output_stream_ << "{";
		m_output_stream_ << R"("cat":"function",)";
		m_output_stream_ << "\"dur\":" << result.end - result.start << ',';
		m_output_stream_ << R"("name":")" << name << "\",";
		m_output_stream_ << R"("ph":"X",)";
		m_output_stream_ << "\"pid\":0,";
		m_output_stream_ << "\"tid\":" << result.thread_id << ",";
		m_output_stream_ << "\"ts\":" << result.start;
		m_output_stream_ << "}";

		m_output_stream_.flush();
	}

	void write_header()
	{
		m_output_stream_ << R"({"otherData": {},"traceEvents":[)";
		m_output_stream_.flush();
	}

	void write_footer()
	{
		m_output_stream_ << "]}";
		m_output_stream_.flush();
	}

	static instrumentor& get()
	{
		static instrumentor instance;
		return instance;
	}
};

class instrumentation_timer
{
public:
	explicit instrumentation_timer(const char* name)
		: m_name_(name), m_stopped_(false)
	{
		m_start_timepoint_ = std::chrono::high_resolution_clock::now();
	}

	~instrumentation_timer()
	{
		try
		{
			if (!m_stopped_)
				stop();
		}
		catch (...)
		{
		}
	}

	void stop()
	{
		const auto end_timepoint = std::chrono::high_resolution_clock::now();

		const auto start = std::chrono::time_point_cast<std::chrono::microseconds>(m_start_timepoint_)
		                   .time_since_epoch().count();
		const auto end = std::chrono::time_point_cast<std::chrono::microseconds>(end_timepoint)
		                 .time_since_epoch().count();

		const uint32_t thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
		instrumentor::get().write_profile({m_name_, start, end, thread_id});

		m_stopped_ = true;
	}

private:
	const char* m_name_;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_start_timepoint_;
	bool m_stopped_;
};
