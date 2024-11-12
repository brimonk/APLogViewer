#include "common.h"
#include "LogEntry.h"
#include "LogFile.h"
#include "StringView.h"
#include "StringMap.h"

namespace APLogViewer
{
	DWORD WINAPI ThreadFunction(LPVOID arg)
	{
		// NOTE (Brian)
		//
		// We want to be able to reload files whenever they change by someone else.
		//
		// According to the Win32 API:
		//
		//     After a file mapping object is created, the size of the file must not exceed the size
		//     of the file mapping object; if it does, not all of the file contents are available
		//     for sharing.
		// 
		//     If an application specifies a size for the file mapping object that is larger than
		//     the size of the actual named file on disk and if the page protection allows write
		//     access (that is, the flProtect parameter specifies PAGE_READWRITE or
		//     PAGE_EXECUTE_READWRITE), then the file on disk is increased to match the specified
		//     size of the file mapping object. If the file is extended, the contents of the file
		//     between the old end of the file and the new end of the file are not guaranteed to be
		//     zero; the behavior is defined by the file system. If the file on disk cannot be
		//     increased, CreateFileMapping fails and GetLastError returns ERROR_DISK_FULL.
		//
		// Given this, the "simplest way" to solve this problem is to have this thread hang around
		// (remember, we create one for every LogFile object), and when the file change notification
		// goes off, we resize our mapping of the file, and parse it starting from the end.
		//
		// This (should be) fine in every single case because our "strings" are actually StringView
		// objects, which are just an offset and a length into the map. Given this, even if Windows
		// gives us a new base pointer, our strings should continue to work just fine.
		//
		// This is, of course, predicated on the idea that only append operations happen. There's
		// never any writes in the middle - according to the APLog spec (source: eng.ms), the
		// logging system will write a _new_ file, with more digits appended at the end.

		LogFile *log_file = reinterpret_cast<LogFile *>(arg);
		while (!log_file->active) {
			::Sleep(50);
		}

		std::cout << "ReadAPLog for " << log_file->path << std::endl;

		log_file->ReadAPLog();
		log_file->WaitForChangesUntilFinished();

		return 0;
	}

	LogFile::LogFile(std::string path, bool *should_run)
	{
		this->path = path;
		this->filename = path.substr(path.find_last_of("/\\") + 1);
		this->should_run = should_run;

		SetupFileMapping();

		this->init_succeeded = true;
	}

	void LogFile::SetupFileMapping()
	{
		std::wstring wpath = std::wstring(this->path.begin(), this->path.end());

		// NOTE I think one of the best ways to handle this error is to have some
		// kind of UI element (big red exclamation mark) with the actual WIN32
		// error code or something that just has this error on display.

		{
			// NOTE We can share files under all circumstances(?) (TESTING)
			//
			// If we DON'T share files under all circumstances, other processes on the machine that
			// request write/delete operations will be prevented from doing so.
			//
			// We don't want to prevent any AP machine from functioning normally (writing logs in
			// this case), we want to just observe what's happening through logs.
			DWORD share_mode = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;

			this->file_handle = ::CreateFile(
				wpath.c_str(),
				GENERIC_READ,
				share_mode,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				nullptr
			);
			if (this->file_handle == INVALID_HANDLE_VALUE) {
				ERR("%s - INVALID_HANDLE_VALUE, GetLastError: %u", this->path.c_str(), ::GetLastError());
				return;
			}
		}

		// TODO (Brian)
		// - detect empty files and prevent mapping them

		{
			this->file_mapping = ::CreateFileMapping(
				this->file_handle, nullptr, PAGE_READONLY, 0, 0, nullptr
			);

			DWORD rc = ::GetLastError();
			if (rc != NO_ERROR && rc == ERROR_ALREADY_EXISTS) {
				ERR("%s - ERROR_ALREADY_EXISTS %u", this->path.c_str(), rc);
				return;
			}

			if (this->file_mapping == nullptr) {
				ERR("%s - file mapping was NULL", this->path.c_str());
				return;
			}
		}

		{
			this->mapping_base = ::MapViewOfFile(
				this->file_mapping, FILE_MAP_READ, 0, 0, 0
			);

			if (this->mapping_base == nullptr) {
				ERR("%s - MapViewOfFile returned NULL", this->path.c_str());
				return;
			}
		}

		{
			LARGE_INTEGER size = { 0 };
			bool rc = GetFileSizeEx(this->file_handle, &size);
			if (!rc) {
				ERR("%s - GetFileSizeEx returned FALSE", this->path.c_str());
				return;
			}

			this->file_size = (u64)size.QuadPart;
		}

		{
			// Get a change notification handle for this file specifically.

			// NOTE We watch for every single event, but we only really expect to see size changes.
			DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME
				| FILE_NOTIFY_CHANGE_DIR_NAME|FILE_NOTIFY_CHANGE_ATTRIBUTES
				| FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_LAST_WRITE
				| FILE_NOTIFY_CHANGE_SECURITY;

			// TODO Make sure we actually pass the entire path.
			// TESTING We pass the current directory

			this->change_notifier = FindFirstChangeNotification(L".\\", false, filter);
			if (this->change_notifier == INVALID_HANDLE_VALUE) {
				ERR("%s - FindFirstChangeNotification returned INVALID_HANDLE_VALUE (FAIL)", this->path.c_str());
			}
		}

	}

	LogFile::~LogFile()
	{
		::WaitForSingleObject(this->thread_handle, INFINITE);
		CloseFileMapping();
	}

	void LogFile::CloseFileMapping()
	{
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
		char *base = (char *)this->mapping_base;
		char *end = base + this->file_size;
		char *next = nullptr;

		for (char *s = (char *)this->mapping_base + this->bytes_read; *this->should_run && s < end; s = next) {
			next = s + GetNextLineEnding(s, (char *)end);
			LogEntry log(s, next - s - 1, base);

			this->entries_mutex.lock();
			this->entries.push_back(log);
			this->entries_mutex.unlock();

			while (isspace(*next))
				next++;

			this->bytes_read = next - (char *)this->mapping_base;
		}
	}

	void LogFile::WaitForChangesUntilFinished()
	{
		while (*this->should_run) {
			DWORD rc = WaitForSingleObject(this->change_notifier, 50);
			if (rc == WAIT_OBJECT_0) {
				// Before we can actually remap the file, we need to determine if _this_ is
				// the file that changed...

				Lock();
				CloseFileMapping();
				SetupFileMapping();
				Unlock();
				ReadAPLog();
			} else if (rc == WAIT_TIMEOUT) {
			}
		}
	}

	void LogFile::Lock()
	{
		entries_mutex.lock();
	}

	void LogFile::Unlock()
	{
		entries_mutex.unlock();
	}

	size_t LogFile::GetNextLineEnding(char *s, char *end)
	{
		char *t;
		for (t = s; t < end && (*t != '\n' && *t != '\r'); t++)
			;
		return t - s;
	}

	StringMap LogFile::GetStringMap(StringView view)
	{
		return StringMap((char *)this->mapping_base, view);
	}
}
