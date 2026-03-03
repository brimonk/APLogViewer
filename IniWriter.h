#ifndef INI_WRITER_H
#define INI_WRITER_H

#include "IniModel.h"

namespace IniWriter {

// Write an IniFile back to a string, preserving original formatting.
std::string WriteToString(const IniFile& ini);

// Write an IniFile to disk. Returns true on success.
bool WriteToFile(const IniFile& ini, const std::string& path, std::string& error_msg);

// Write to the file's original path (ini.file_path). Returns true on success.
bool Save(const IniFile& ini, std::string& error_msg);

} // namespace IniWriter

#endif // INI_WRITER_H
