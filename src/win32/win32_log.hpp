#pragma once
#ifndef WIN32_LOG_HPP_INCLUDED
#define WIN32_LOG_HPP_INCLUDED
#include <stdio.h>
#include "win32_platform.h"

// TODO(james): support multiple log levels

#define LOG(msg, ...) Win32Log(__FILE__, __LINE__, msg, __VA_ARGS__)
#define LOG_DEBUG(msg, ...) LOG(msg, __VA_ARGS__)
#define LOG_INFO(msg, ...) LOG(msg, __VA_ARGS__)
#define LOG_WARN(msg, ...) LOG(msg, __VA_ARGS__)
#define LOG_ERROR(msg, ...) LOG(msg, __VA_ARGS__)

void
Win32Log(const char* file, int lineno, const char* format, ...)
{
    va_list args;
    va_start(args, format);

    char logMessage[512];
    vsprintf_s(logMessage, format, args);
    ASSERT(n < 512);    // overflowed the log buffer

    va_end(args);

    char logLine[1024];
    sprintf_s(logLine, "%s(%d): %s\n", file, lineno, logMessage);

    // TODO(james): use console and/or log file
    OutputDebugStringA(logLine);
}

#endif