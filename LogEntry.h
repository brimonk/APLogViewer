#ifndef LOG_ENTRY_H
#define LOG_ENTRY_H

#include "common.h"

namespace APLogViewer
{
    class LogEntry {
    public:
        char level;
        u64 date_timestamp;
        u64 service;
        u64 tag;
        u64 source_file;
        u64 source_function;
        i64 source_line;
        i64 process_id;
        i64 thread_id;
        u64 timestamp;
        u64 message_id;
    
        LogEntry(char *s);
    };

}

#endif // LOG_ENTRY_H