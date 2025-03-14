#ifndef _GOODIX_LOG_H_
#define _GOODIX_LOG_H_
#include "goodix_type.h"

typedef int32_t(*CallbackPrintfUser)(const char *, ...);
typedef int32_t(*CallbackSnprintfUser)(char *str, size_t size, const char *format, ...);
typedef void(*Callback_GH3X2X_Log)(GCHAR *log_string);

DRVDLL_API void goodix_set_sprintFunc(CallbackPrintfUser g_pPrintfUserIn, CallbackSnprintfUser g_pSnprintfUserIn, Callback_GH3X2X_Log GH3X2X_Log_In);

//log print
//extern CallbackPrintfUser g_pPrintfUser;
//extern CallbackSnprintfUser g_pSnprintfUser;
//extern Callback_GH3X2X_Log GH3X2X_PlatformLog;

///// log support len
//#define GH3X2X_LOG_DEBUG_SUP_LEN     (128)

#endif // !_GOODIX_LOG_H_
