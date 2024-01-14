#include "common.h"

#define USAGE "USAGE: %s <file.log>"

int main(int argc, char **argv)
{
	if (argc < 2) {
		ERR(USAGE "\n", argv[0]);
		return 1;
	}

	char *fname = argv[1];
	FILE *fp = fopen(fname, "ab");

	const char *service = "LogGenerator";
	const char *tag = "SampleGenerator";

	u32 pid = GetProcessId(GetCurrentProcess());
	u32 tid = GetCurrentThreadId();
	FILETIME filetime;
	u64 ts;

	for (i32 i = 0; i < 1000; i++) {
		char timebuf[32] = { 0 };
		char logmsg[64] = { 0 };
		char level = 'i';

		time_t the_time = time(NULL);
		struct tm *curr_tm = localtime(&the_time);

		strftime(timebuf, sizeof timebuf, "%d/%m/%Y %H:%M:%S", curr_tm);

		GetSystemTimeAsFileTime(&filetime);

		ts = ((u64)filetime.dwHighDateTime << 32) | filetime.dwLowDateTime;

		snprintf(logmsg, sizeof logmsg, "i = %d", i);

		fprintf(fp,
			"%c,%s %s,%s SrcFile=\"%s\" SrcFunc=\"%s\" SrcLine=\"%d\" "
			"Pid=\"%d\" Tid=\"%d\" TS=\"0x%llx\" String1=\"%s\"",
			level,
			timebuf,
			service,
			tag,
			__FILE__,
			__FUNCTION__,
			__LINE__,
			pid,
			tid,
			ts,
			logmsg
		);

		fprintf(fp, "\n");
	}

	fclose(fp);

	return 0;
}
