
// TODO(james): support multiple log levels

#if defined(PROJECTSUPER_INTERNAL)
#define LOG(level, msg, ...) Win32DebugLog(level, __FILE__, __LINE__, msg, __VA_ARGS__)
#define LOG_DEBUG(msg, ...) LOG(LogLevel::Debug, msg, __VA_ARGS__)
#define LOG_INFO(msg, ...) LOG(LogLevel::Info, msg, __VA_ARGS__)
#define LOG_ERROR(msg, ...) LOG(LogLevel::Error, msg, __VA_ARGS__)
#else
#define LOG(level, msg, ...) Win32Log(level, msg, __VA_ARGS__)
#define LOG_DEBUG(msg, ...) 
#define LOG_INFO(msg, ...) LOG(LogLevel::Info, msg, __VA_ARGS__)
#define LOG_ERROR(msg, ...) LOG(LogLevel::Error, msg, __VA_ARGS__)
#endif

internal void
Win32Log(LogLevel level, const char* format, ...)
{
    va_list args, args2;
    va_start(args, format);
    va_copy(args2, args);

    int n = FormatStringV(0, format, args);
    va_end(args);
    
    char* szMessage = (char*)malloc(n);
    FormatStringV(szMessage, format, args2);
    va_end(args2);

    switch(level)
    {
    case LogLevel::Debug:
        OutputDebugStringA("DBG | ");
        break;
    case LogLevel::Info:
        OutputDebugStringA("INF | ");
        break;
    case LogLevel::Error:
        OutputDebugStringA("ERR | ");
        break;
    }

    OutputDebugStringA(szMessage);
    OutputDebugStringA("\n");

    free(szMessage);
}

internal void
Win32DebugLog(LogLevel level, const char* file, int lineno, const char* format, ...)
{
    va_list args;
    va_start(args, format);

    char logMessage[2048];
    int n = FormatStringV(logMessage, format, args);

    va_end(args);

    char logLine[512];
    ps_sprintf(logLine, "%s(%d) | ", file, lineno);

    
    switch(level)
    {
    case LogLevel::Debug:
        OutputDebugStringA("DBG | ");
        break;
    case LogLevel::Info:
        OutputDebugStringA("INF | ");
        break;
    case LogLevel::Error:
        OutputDebugStringA("ERR | ");
        break;
    }

    // TODO(james): use console and/or log file
    OutputDebugStringA(logLine);
    OutputDebugStringA(logMessage);
    OutputDebugStringA("\n");
}
