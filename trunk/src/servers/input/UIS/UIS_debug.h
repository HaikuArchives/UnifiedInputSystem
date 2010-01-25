#ifndef _UIS_DEBUG_H
#define _UIS_DEBUG_H

#define TRACE_IS_UIS

#undef TRACE
#ifdef TRACE_IS_UIS
void uis_log_print(const char *fmt, ...);
#define TRACE(a...) uis_log_print(a)
#else
#define TRACE(a...)
#endif


#endif // _UIS_DEBUG_H
