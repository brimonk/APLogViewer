#include "common.h"
#include "LogEntry.h"
#include "StringView.h"

extern std::unordered_map<u64, std::string> STRING_MAP;
extern std::mutex GLOBAL_STATE_MUTEX;
extern bool g_ReadInput;

namespace APLogViewer
{
    u64 hash(char *str)
    {
        u64 hash = 5381;
        int c;
        while (c = *str++) {
            hash = ((hash << 5) + hash) + c;
        }
        return hash;
    }

    u64 map_upsert(std::string str)
    {
        u64 h = hash((char *)str.c_str());
        GLOBAL_STATE_MUTEX.lock();
        auto search = STRING_MAP.find(h);
        if (search == STRING_MAP.end()) {
            STRING_MAP.insert({ h, str });
        }
        GLOBAL_STATE_MUTEX.unlock();
        return h;
    }

    std::string get_string(u64 h)
    {
        GLOBAL_STATE_MUTEX.lock();
        auto search = STRING_MAP.find(h);
        std::string result = search != STRING_MAP.end() ? search->second : std::string();
        GLOBAL_STATE_MUTEX.unlock();
        return result;
    }

    u64 parse_timestamp(std::string timestamp)
    {
        u64 ts = 0;
        int year, month, day, hour, minute, second;
        int rc = sscanf(timestamp.c_str(), "%d/%d/%d %d:%d:%d",
            &month, &day, &year, &hour, &minute, &second);
        if (rc == 6) {
            struct tm tt = { 0 };
            tt.tm_year = year - 1900;
            tt.tm_mon = month - 1;
            tt.tm_mday = day;
            tt.tm_hour = hour;
            tt.tm_min = minute;
            tt.tm_sec = second;
            ts = mktime(&tt);
        }
        return ts;
    }

    u64 parse_hex_timestamp(const char *str)
    {
#define WINDOWS_TICK 10000000
#define SEC_TO_UNIX_EPOCH 11644473600LL

        const char *key = "TS=\"";
        char *s = (char *)strstr(str, key);
        if (s) {
            s += strlen(key);
            u64 ticks = (u64)strtoll(s, nullptr, 16);
            // convert to unix time

            u64 unix_ts = (ticks / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);
            return unix_ts;
        } else {
            return 0;
        }

#undef WINDOWS_TICK
#undef SEC_TO_UNIX_EPOCH
    }

    std::string ReadStringWithKey(const char *str, std::string key)
    {
        char tkey[32];
        snprintf(tkey, sizeof tkey, "%s=\"", key.c_str());
        char *s = (char *)strstr(str, tkey);
        if (s == NULL) {
            return std::string();
        }
        s += strlen(tkey);
        const char *e = strchr(s, '"');
        return std::string(s, e - s);
    }

    int ReadIntegerWithKey(const char *str, std::string key)
    {
        char tkey[32];
        snprintf(tkey, sizeof tkey, "%s=\"", key.c_str());
        const char *s = strstr(str, tkey) + strlen(tkey);
        return atoi(s);
    }

	LogEntry::LogEntry(char *s, size_t len, char *base)
	{
        std::string str(s, len);

        this->level = s[0];
        std::string date_timestamp = str.substr(2, sizeof("dd-mm-YYYY HH:MM:SS") - 1);
        this->date_timestamp = parse_timestamp(date_timestamp);

        const char *src1_ptr = s + 22;
        size_t src1_len = strchr(src1_ptr, ',') - src1_ptr;
        std::string service = str.substr(src1_ptr - s, src1_len);
        this->service = map_upsert(service);

        const char *src2_ptr = src1_ptr + src1_len + 1;
        size_t src2_len = strchr(src2_ptr, ',') - src2_ptr;
        std::string tag = str.substr(src2_ptr - s, src2_len);
        this->tag = map_upsert(tag);

        std::string source_file = ReadStringWithKey(s, "SrcFile");
        this->source_file = map_upsert(source_file);
        std::string source_function = ReadStringWithKey(s, "SrcFunc");
        this->source_function = map_upsert(source_function);
        this->source_line = ReadIntegerWithKey(s, "SrcLine");

        this->process_id = ReadIntegerWithKey(s, "Pid");
        this->thread_id = ReadIntegerWithKey(s, "Tid");
        
        this->timestamp = parse_hex_timestamp(s);

        const char *string1 = strstr(s, "String1");
        string1 += strlen("String1") + 2;

        // copy everything but the first and last quotes

        std::string message = str.substr(string1 - s, len - (size_t)(string1 - s));
        this->message_id = map_upsert(message);

        u64 offset = (u64)(string1 - base);
        u64 length = (u64)(len - (u64)(string1 - s));

        this->message = StringView(offset, length);
	}
}
