#include "common.h"
#include "LogEntry.h"
#include "StringView.h"

extern bool g_ReadInput;

namespace APLogViewer
{
    u64 parse_timestamp(char *s)
    {
        u64 ts = 0;
        int year, month, day, hour, minute, second;

        char timebuf[32] = { 0 };
        memcpy(timebuf, s, MIN(sizeof timebuf, strchr(s, ',') - s));

        int rc = sscanf(timebuf, "%d/%d/%d %d:%d:%d",
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

    StringView ReadStringWithKey(const char *str, const char *key, char *base)
    {
        char tkey[32];
        snprintf(tkey, sizeof tkey, "%s=\"", key);
        char *s = (char *)strstr(str, tkey);
        if (s == NULL) {
            return StringView();
        }
        s += strlen(tkey);
        const char *e = strchr(s, '"');

        u64 offset = s - base;
        u64 length = e - s;

        return StringView(offset, length);
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

        // TODO handle cases where log files are incomplete
        // - line writing at the end wasn't flushed all the way, etc.

        this->level = s[0];
        this->date_timestamp = parse_timestamp(s + 2);

        const char *src1_ptr = s + 22;
        size_t src1_len = strchr(src1_ptr, ',') - src1_ptr;
        this->service = StringView(src1_ptr - base, src1_len);

        const char *src2_ptr = src1_ptr + src1_len + 1;
        size_t src2_len = strchr(src2_ptr, ',') - src2_ptr;
        this->tag = StringView(src2_ptr - base, src2_len);

        this->source_file = ReadStringWithKey(s, "SrcFile", base);
        this->source_function = ReadStringWithKey(s, "SrcFunc", base);
        this->source_line = ReadIntegerWithKey(s, "SrcLine");

        this->process_id = ReadIntegerWithKey(s, "Pid");
        this->thread_id = ReadIntegerWithKey(s, "Tid");
        
        this->timestamp = parse_hex_timestamp(s);

        const char *string1 = strstr(s, "String1");
        string1 += strlen("String1") + 2;

        // the "log message" is everything between the first and last pair of '"'
        // (hence the + 2 previously)

        u64 offset = (u64)(string1 - base);
        u64 length = (u64)(len - (u64)(string1 - s));

        this->message = StringView(offset, length);
	}
}
