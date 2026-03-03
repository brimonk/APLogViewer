#ifndef INI_MODEL_H
#define INI_MODEL_H

#include "common.h"

// --- Condition Scopes ---

enum ScopeType {
    Scope_Machine,
    Scope_MF,
    Scope_Cluster,
    Scope_Environment,
};

// Returns the precedence for a scope type (lower = higher priority)
inline int ScopePrecedence(ScopeType t)
{
    switch (t) {
    case Scope_Machine:     return 1;
    case Scope_MF:          return 2;
    case Scope_Cluster:     return 3;
    case Scope_Environment: return 4;
    default:                return 99;
    }
}

inline const char* ScopeTypeName(ScopeType t)
{
    switch (t) {
    case Scope_Machine:     return "Machine";
    case Scope_MF:          return "MF";
    case Scope_Cluster:     return "Cluster";
    case Scope_Environment: return "Environment";
    default:                return "Unknown";
    }
}

// Try to parse a scope type name. Returns true on success.
inline bool ParseScopeType(const std::string& name, ScopeType& out)
{
    if (name == "Machine")     { out = Scope_Machine;     return true; }
    if (name == "MF")          { out = Scope_MF;          return true; }
    if (name == "Cluster")     { out = Scope_Cluster;     return true; }
    if (name == "Environment") { out = Scope_Environment; return true; }
    return false;
}

struct Condition {
    ScopeType scope_type;
    std::string scope_value;
};

// --- Key Entry ---

struct KeyEntry {
    // Conditions that must match for this key to apply (empty = unconditional)
    std::vector<Condition> conditions;
    // The key name
    std::string key;
    // The value
    std::string value;
    // Inline comment (if any, empty string if none)
    std::string inline_comment;
};

// --- Section Types ---

enum SectionType {
    Section_PerfConfig,
    Section_MDM,
    Section_DSTS,
    Section_Producer,
    Section_GWProducer,
    Section_Consumer,
    Section_Reliable,
    Section_Rehydrate,
    Section_Unknown,
};

inline const char* SectionTypeName(SectionType t)
{
    switch (t) {
    case Section_PerfConfig: return "PerfConfig";
    case Section_MDM:        return "MDM";
    case Section_DSTS:       return "DSTS";
    case Section_Producer:   return "Producer";
    case Section_GWProducer: return "GWProducer";
    case Section_Consumer:   return "Consumer";
    case Section_Reliable:   return "Reliable";
    case Section_Rehydrate:  return "Rehydrate";
    case Section_Unknown:    return "Unknown";
    default:                 return "Unknown";
    }
}

// Classify a section header name into a SectionType
inline SectionType ClassifySectionType(const std::string& name)
{
    if (name == "PerfConfig") return Section_PerfConfig;
    if (name == "MDM")        return Section_MDM;
    if (name == "DSTS")       return Section_DSTS;

    // Check prefixes for scenario types
    if (name.rfind("GWProducer_", 0) == 0) return Section_GWProducer;
    if (name.rfind("Producer_", 0) == 0)   return Section_Producer;
    if (name.rfind("Consumer_", 0) == 0)   return Section_Consumer;
    if (name.rfind("Reliable_", 0) == 0)   return Section_Reliable;
    if (name.rfind("Rehydrate_", 0) == 0)  return Section_Rehydrate;

    return Section_Unknown;
}

// Extract just the scenario name from a section header (e.g., "Producer_Foo" -> "Foo")
inline std::string ExtractScenarioName(const std::string& section_name)
{
    size_t pos = section_name.find('_');
    if (pos != std::string::npos && pos + 1 < section_name.size())
        return section_name.substr(pos + 1);
    return section_name;
}

// --- Lines ---

enum LineType {
    Line_KeyValue,
    Line_Comment,
    Line_Blank,
};

struct Line {
    LineType type;
    KeyEntry key_entry;      // Valid when type == Line_KeyValue
    std::string comment;     // Valid when type == Line_Comment (includes leading ';')
};

// --- Section ---

struct Section {
    // Section header name (e.g., "PerfConfig", "Producer_AzPubSubPerf-5-2")
    std::string name;
    // Classified type
    SectionType section_type;
    // Lines within this section (keys, comments, blanks) in order
    std::vector<Line> lines;
};

// --- INI File ---

struct IniFile {
    // Preamble comments/blanks before any section
    std::vector<Line> preamble;
    // All sections in file order
    std::vector<Section> sections;
    // Source file path
    std::string file_path;
};

#endif // INI_MODEL_H
