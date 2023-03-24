#include <stdarg.h>
#include "log.h"

#pragma warning(disable : 4996)

CbpLogger::CbpLogger(const char* fname)
{
#ifdef LOG_ON
    handle = fopen(fname, "a");
    if (handle)
    {
        fprintf(handle, "CBP Log initialized\n");
    }
#endif
}

void CbpLogger::Verbose(const char* args...)
{
    if (handle)
    {
        va_list argptr;
        va_start(argptr, args);
        vfprintf(handle, args, argptr);
        va_end(argptr);
        fflush(handle);
    }
}

void CbpLogger::Info(const char* args...)
{
    if (handle)
    {
        va_list argptr;
        va_start(argptr, args);
        vfprintf(handle, args, argptr);
        va_end(argptr);
        fflush(handle);
    }
}

void CbpLogger::Error(const char* args...)
{
    if (handle)
    {
        va_list argptr;
        va_start(argptr, args);
        vfprintf(handle, args, argptr);
        va_end(argptr);
        fflush(handle);
    }
}

CbpLogger logger("Data\\F4SE\\Plugins\\cbpc.log");