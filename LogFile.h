#ifndef LOG_FILE_H
#define LOG_FILE_H

#include "common.h"

namespace APLogViewer
{
	class LogFile
	{
	public:
		std::string path;
		bool *should_run = nullptr;
		std::vector<LogEntry> entries;
		std::mutex entries_mutex;

		LogFile(std::string path, bool *should_run);
		~LogFile();

		bool Start();
		u64 GetEntriesCount();
		void ReadAPLog();

	private:
		HANDLE thread_handle = nullptr;
	};
}

#endif // LOG_FILE_H