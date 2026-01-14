#ifndef _DEBUG_LOG_H_
#define _DEBUG_LOG_H_
#include <stdarg.h>

typedef void (*ErrMsgCallback_t) (const char *fmt,va_list fmtargs);
// typedef void (ErrMsgCallback_t) (const char *format);

void PianoRegisterErrMsgCallback(ErrMsgCallback_t);
void PianoPrintErrMsg(const char *format,...) __attribute__((format(printf, 1, 2)));

#define ELOG(format, ... ) PianoPrintErrMsg("%s#%d: " format, __FUNCTION__,__LINE__,## __VA_ARGS__)
#ifdef DEBUG
   #define LOG(format, ... ) printf("%s: " format,__FUNCTION__,## __VA_ARGS__)
   #define LOG_RAW(format, ... ) printf(format,## __VA_ARGS__)
#else
   #define LOG(format, ... )
   #define LOG_RAW(format, ... )
#endif

#endif

