#include "IniParser.h"
#include <fstream>
#include <sstream>

// --- Internal helpers ---

static std::string TrimRight(const std::string& s)
{
    size_t end = s.find_last_not_of(" \t\r\n");
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

static std::string TrimLeft(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t");
    return (start == std::string::npos) ? "" : s.substr(start);
}

static std::string Trim(const std::string& s)
{
    return TrimLeft(TrimRight(s));
}

// Parse conditions from the prefix portion (everything before '$').
// Handles two formats:
//   1. Explicit: "Cluster:AMS02P,Environment:AzPubSub2-Dev-AMS02P"
//   2. Shorthand: "AM3P#AzPubSub1-Dev-AM3P" (Cluster#Environment)
static bool ParseConditions(const std::string& prefix, std::vector<Condition>& conditions)
{
    if (prefix.empty())
        return true;

    // Check which format we're dealing with.
    // Shorthand uses '#' and contains no ':' scope type prefix.
    // Explicit uses 'ScopeType:Value' separated by commas.
    bool has_colon_scope = false;
    {
        // Quick check: does any segment start with a known scope type + ':'?
        // We look for "Cluster:", "Environment:", "MF:", "Machine:" anywhere in the prefix
        if (prefix.find("Cluster:") != std::string::npos ||
            prefix.find("Environment:") != std::string::npos ||
            prefix.find("MF:") != std::string::npos ||
            prefix.find("Machine:") != std::string::npos)
        {
            has_colon_scope = true;
        }
    }

    if (!has_colon_scope && prefix.find('#') != std::string::npos) {
        // Shorthand format: "ClusterName#EnvironmentName"
        // Can also just be "ClusterName" with no '#'
        size_t hash_pos = prefix.find('#');
        std::string cluster_val = prefix.substr(0, hash_pos);
        std::string env_val = prefix.substr(hash_pos + 1);

        if (!cluster_val.empty()) {
            Condition c;
            c.scope_type = Scope_Cluster;
            c.scope_value = cluster_val;
            conditions.push_back(c);
        }
        if (!env_val.empty()) {
            Condition c;
            c.scope_type = Scope_Environment;
            c.scope_value = env_val;
            conditions.push_back(c);
        }
        return true;
    }

    // Explicit format: comma-separated "ScopeType:Value" pairs
    // Split on commas
    std::stringstream ss(prefix);
    std::string segment;
    while (std::getline(ss, segment, ',')) {
        segment = Trim(segment);
        if (segment.empty())
            continue;

        size_t colon_pos = segment.find(':');
        if (colon_pos == std::string::npos) {
            // Not a recognized format — could be a bare value
            // Treat as cluster name for backward compatibility
            Condition c;
            c.scope_type = Scope_Cluster;
            c.scope_value = segment;
            conditions.push_back(c);
            continue;
        }

        std::string scope_name = segment.substr(0, colon_pos);
        std::string scope_val = segment.substr(colon_pos + 1);

        ScopeType st;
        if (!ParseScopeType(scope_name, st)) {
            // Unknown scope type — store as-is with Unknown/Cluster fallback
            // This shouldn't normally happen with well-formed files
            return false;
        }

        Condition c;
        c.scope_type = st;
        c.scope_value = scope_val;
        conditions.push_back(c);
    }

    return true;
}

// Parse a single key-value line (possibly with conditions).
// The line should already be trimmed of leading/trailing whitespace.
// Returns false if the line cannot be parsed as a key-value.
static bool ParseKeyValueLine(const std::string& line, KeyEntry& entry)
{
    // Find the first '=' to split key and value
    size_t eq_pos = line.find('=');
    if (eq_pos == std::string::npos)
        return false;

    std::string key_part = line.substr(0, eq_pos);
    std::string value_part = line.substr(eq_pos + 1);

    // Check for inline comment in value ('; comment' after value)
    // Be careful not to match semicolons inside values.
    // Heuristic: look for " ;" pattern (space + semicolon)
    entry.inline_comment.clear();
    {
        // Only consider inline comments if there's a space-semicolon pattern
        // that isn't inside the value itself. Simple approach: find last " ;"
        size_t comment_pos = value_part.rfind(" ;");
        if (comment_pos != std::string::npos) {
            entry.inline_comment = value_part.substr(comment_pos + 1);
            value_part = TrimRight(value_part.substr(0, comment_pos));
        }
    }

    // Check if key_part has a '$' indicating conditions
    entry.conditions.clear();
    size_t dollar_pos = key_part.find('$');
    if (dollar_pos != std::string::npos) {
        std::string condition_str = key_part.substr(0, dollar_pos);
        std::string actual_key = key_part.substr(dollar_pos + 1);

        if (!ParseConditions(condition_str, entry.conditions))
            return false;

        entry.key = actual_key;
    } else {
        entry.key = key_part;
    }

    entry.value = value_part;

    return true;
}

// Core parsing logic that works on a vector of lines
static bool ParseLines(const std::vector<std::string>& raw_lines, IniFile& out, std::string& error_msg)
{
    out.preamble.clear();
    out.sections.clear();

    Section* current_section = nullptr;
    int line_num = 0;

    for (const auto& raw_line : raw_lines) {
        line_num++;

        // Strip trailing \r\n but preserve the content
        std::string line = TrimRight(raw_line);

        // Determine the target list for this line (preamble or current section)
        auto& target = current_section ? current_section->lines : out.preamble;

        // Blank line — skip
        if (line.empty() || Trim(line).empty()) {
            continue;
        }

        std::string trimmed = Trim(line);

        // Comment line (starts with ';') — skip
        if (trimmed[0] == ';') {
            continue;
        }

        // Section header
        if (trimmed[0] == '[') {
            size_t close = trimmed.find(']');
            if (close == std::string::npos) {
                error_msg = "Line " + std::to_string(line_num) + ": unclosed section header";
                return false;
            }

            std::string section_name = trimmed.substr(1, close - 1);

            Section sec;
            sec.name = section_name;
            sec.section_type = ClassifySectionType(section_name);
            out.sections.push_back(sec);
            current_section = &out.sections.back();
            continue;
        }

        // Key-value line (possibly with conditions)
        {
            Line l;
            l.type = Line_KeyValue;
            if (!ParseKeyValueLine(line, l.key_entry)) {
                // If we can't parse it as key=value, treat it as a comment
                // (some INI files have malformed lines)
                l.type = Line_Comment;
                l.comment = line;
            }
            target.push_back(l);
        }
    }

    return true;
}

// --- Public API ---

bool IniParser::ParseString(const std::string& content, IniFile& out, std::string& error_msg)
{
    // Split into lines
    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }

    return ParseLines(lines, out, error_msg);
}

bool IniParser::ParseFile(const std::string& path, IniFile& out, std::string& error_msg)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        error_msg = "Failed to open file: " + path;
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    out.file_path = path;
    return ParseString(content, out, error_msg);
}
