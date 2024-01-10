#ifndef LOG_ENTRY_H
#define LOG_ENTRY_H

#include "common.h"
#include "StringView.h"

namespace APLogViewer
{
    class LogEntry {
    public:
        char level;
        u64 date_timestamp;
        StringView service;
        StringView tag;
        StringView source_file;
        StringView source_function;
        i64 source_line;
        i64 process_id;
        i64 thread_id;
        u64 timestamp;
        u64 timestamp_ns;
        u64 message_id;
        StringView message;
    
        LogEntry(char *s, size_t len, char *base);
    };

}

#endif // LOG_ENTRY_H
