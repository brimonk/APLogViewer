#include "common.h"
#include "LogEntry.h"
#include "LogFile.h"

namespace APLogViewer
{
	DWORD WINAPI ThreadFunction(LPVOID arg)
	{
		LogFile *log_file = reinterpret_cast<LogFile *>(arg);
		log_file->ReadAPLog();
		return 0;
	}

	LogFile::LogFile(std::string path, bool *should_run)
	{
		this->path = path;
		this->filename = path.substr(path.find_last_of("/\\") + 1);
		this->should_run = should_run;

		std::wstring wpath = std::wstring(this->path.begin(), this->path.end());

		// NOTE We only open these files for reading both during CreateFile, and
		// CreateFileMapping. We don't want to accidentally WRECK our log files.

		// NOTE I think one of the best ways to handle this error is to have some
		// kind of UI element (big red exclamation mark) with the actual WIN32
		// error code or something that just has this error on display.

		{
			this->file_handle = ::CreateFile(
				wpath.c_str(),
				GENERIC_READ,
				FILE_SHARE_READ,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				nullptr
			);
			if (this->file_handle == INVALID_HANDLE_VALUE) {
				// best way to handle this error?
				return;
			}
		}

		{
			this->file_mapping = ::CreateFileMapping(
				this->file_handle, nullptr, PAGE_READONLY, 0, 0, nullptr
			);

			DWORD rc = ::GetLastError();
			if (rc != NO_ERROR && rc == ERROR_ALREADY_EXISTS) {
				// best way to handle this error?
				return;
			}

			if (this->file_mapping == nullptr) {
				// MSDN docs from the previous if block were wrong... How silly.
				return;
			}
		}

		{
			this->mapping_base = ::MapViewOfFile(
				this->file_mapping, FILE_MAP_READ, 0, 0, 0
			);

			if (this->mapping_base == nullptr) {
				// Couldn't map the file into memory.
				return;
			}
		}
	}

	LogFile::~LogFile()
	{
		*this->should_run = false;
		::WaitForSingleObject(this->thread_handle, INFINITE);

		::UnmapViewOfFile(this->mapping_base);

		::CloseHandle(this->file_mapping);
		::CloseHandle(this->file_handle);
	}

	bool LogFile::Start()
	{
		this->thread_handle = ::CreateThread(NULL, 0, ThreadFunction, (LPVOID)this, 0, nullptr);
		return true;
	}

	u64 LogFile::GetEntriesCount()
	{
		this->entries_mutex.lock();
		u64 value = this->entries.size();
		this->entries_mutex.unlock();
		return value;
	}

	void LogFile::ReadAPLog()
	{
		std::ifstream file(this->path);
		std::string str;

		// i,11/28/2023 12:42:09,AzPubSubPerf,DefaultTag,SrcFile="" SrcFunc="" SrcLine="0" Pid="5640" Tid="2324" TS="0x01DA223B5BB194F2" String1="Setting azpubsub.kusto.log.level: 7"

		while (this->should_run && std::getline(file, str)) {
			// NOTE Make sure that each line ends in a '"'
			// there's probably a better way to determine if we have a partial line or not...
			if (str[str.length() - 1] != '"')
				continue;

			LogEntry log((char *)str.c_str());

			this->entries_mutex.lock();
			this->entries.push_back(log);
			this->entries_mutex.unlock();
		}
	}
}
