#ifndef LOG_FILE_H
#define LOG_FILE_H

#include "common.h"

namespace APLogViewer
{
	class LogFile
	{
	public:
		std::string path;
		std::string filename;
		bool *should_run = nullptr;
		std::vector<LogEntry> entries;
		std::mutex entries_mutex;
		bool init_succeeded = false;
		u64 file_size;

		LogFile(std::string path, bool *should_run);
		~LogFile();

		bool Start();
		u64 GetEntriesCount();
		void ReadAPLog();

	private:
		size_t GetNextLineEnding(char *s, char *end);

		HANDLE thread_handle = nullptr;
		HANDLE file_handle = nullptr;
		HANDLE file_mapping = nullptr;
		LPVOID mapping_base = nullptr;
	};
}

#endif // LOG_FILE_H
