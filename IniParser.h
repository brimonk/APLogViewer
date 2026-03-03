#ifndef INI_PARSER_H
#define INI_PARSER_H

#include "IniModel.h"

namespace IniParser {

// Parse an INI file from disk. Returns true on success.
// On failure, sets error_msg and returns false.
bool ParseFile(const std::string& path, IniFile& out, std::string& error_msg);

// Parse INI content from a string buffer.
bool ParseString(const std::string& content, IniFile& out, std::string& error_msg);

} // namespace IniParser

#endif // INI_PARSER_H
