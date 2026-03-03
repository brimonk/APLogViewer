#include "IniWriter.h"
#include <fstream>
#include <sstream>

// Reconstruct a single key-value line from a KeyEntry, including conditions.
static std::string FormatKeyEntry(const KeyEntry& entry)
{
    std::string result;

    // Write conditions prefix if any
    if (!entry.conditions.empty()) {
        for (size_t i = 0; i < entry.conditions.size(); i++) {
            if (i > 0)
                result += ',';
            result += ScopeTypeName(entry.conditions[i].scope_type);
            result += ':';
            result += entry.conditions[i].scope_value;
        }
        result += '$';
    }

    // Key=Value
    result += entry.key;
    result += '=';
    result += entry.value;

    // Inline comment
    if (!entry.inline_comment.empty()) {
        result += ' ';
        result += entry.inline_comment;
    }

    return result;
}

// Write a vector of lines to the output stream
static void WriteLines(std::ostringstream& out, const std::vector<Line>& lines)
{
    for (const auto& line : lines) {
        switch (line.type) {
        case Line_Blank:
            out << '\n';
            break;
        case Line_Comment:
            out << line.comment << '\n';
            break;
        case Line_KeyValue:
            out << FormatKeyEntry(line.key_entry) << '\n';
            break;
        }
    }
}

std::string IniWriter::WriteToString(const IniFile& ini)
{
    std::ostringstream out;

    // Write preamble
    WriteLines(out, ini.preamble);

    // Write sections
    for (const auto& section : ini.sections) {
        out << '[' << section.name << ']' << '\n';
        WriteLines(out, section.lines);
    }

    return out.str();
}

bool IniWriter::WriteToFile(const IniFile& ini, const std::string& path, std::string& error_msg)
{
    std::string content = WriteToString(ini);

    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        error_msg = "Failed to open file for writing: " + path;
        return false;
    }

    file << content;
    file.close();

    if (file.fail()) {
        error_msg = "Error writing to file: " + path;
        return false;
    }

    return true;
}

bool IniWriter::Save(const IniFile& ini, std::string& error_msg)
{
    if (ini.file_path.empty()) {
        error_msg = "No file path set";
        return false;
    }
    return WriteToFile(ini, ini.file_path, error_msg);
}
