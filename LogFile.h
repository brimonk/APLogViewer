#ifndef LOG_FILE_H
#define LOG_FILE_H

#include "common.h"
#include "StringView.h"
#include "StringMap.h"

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
		u64 bytes_read;
		u64 file_size;

		LogFile(std::string path, bool *should_run);
		~LogFile();

		bool Start();
		u64 GetEntriesCount();
		void ReadAPLog();

		void WaitForChangesUntilFinished();

		void SetupFileMapping();
		void CloseFileMapping();

		void Lock();
		void Unlock();

		StringMap GetStringMap(StringView view);

	private:
		size_t GetNextLineEnding(char *s, char *end);

		HANDLE change_notifier = nullptr;
		HANDLE thread_handle = nullptr;
		HANDLE file_handle = nullptr;
		HANDLE file_mapping = nullptr;
		LPVOID mapping_base = nullptr;
	};
}

#endif // LOG_FILE_H
