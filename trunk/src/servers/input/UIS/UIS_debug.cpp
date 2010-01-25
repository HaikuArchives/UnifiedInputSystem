#include "UIS_debug.h"


#ifdef TRACE_IS_UIS
#include <stdio.h>
void uis_log_print(const char *fmt, ...)
{
	FILE* log = fopen("/var/log/uis_input_server.log", "a");
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	fputs(buf, log);
	fflush(log);
	fclose(log);
}
#endif
