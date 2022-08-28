#pragma once
#include <stdio.h>
#include <shared_mutex>

class CbpLogger
{
public:
    CbpLogger(const char* fname);
    void Info(const char* args...);
    void Error(const char* args...);

    FILE* handle;

    std::shared_mutex log_lock;
};

extern CbpLogger logger;