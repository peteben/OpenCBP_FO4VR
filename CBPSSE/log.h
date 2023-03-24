#pragma once
#include <stdio.h>
#include <shared_mutex>

#define LOG_ON

class CbpLogger
{
public:
    CbpLogger(const char* fname);
    void Verbose(const char* args...);
    void Info(const char* args...);
    void Error(const char* args...);

    FILE* handle;

    std::shared_mutex log_lock;
};

extern CbpLogger logger;

#define LOG_VERBOSE(fmt, ...)

#ifdef LOG_ON
//#define LOG_VERBOSE logger.Verbose
#define LOG_INFO logger.Info
#define LOG_ERROR logger.Error
#else
#define LOG_INFO(fmt, ...)
#define LOG_ERROR(fmt, ...)
#endif
