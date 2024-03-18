#pragma once
#include <cstdarg>

#if defined(CT_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#else
#define DBG_NEW new
#endif

#if defined(CT_DEBUG) || defined(CT_OPTDEBUG)
#define DEBUG_ASSERT(condition, message, ...) { Debug::Assert((condition), (message), __VA_ARGS__); }
#define DEBUG_ASSERT_N(condition) { Debug::Assert((condition), "Assert Failed"); }
#define DEBUG_LOG_CRITICAL(message, ...) { Debug::Log(Debug::Severity::Critical, (message), __VA_ARGS__); }
#define DEBUG_LOG_ERROR(message, ...) { Debug::Log(Debug::Severity::Error, (message), __VA_ARGS__); }
#define DEBUG_LOG_WARNING(message, ...) { Debug::Log(Debug::Severity::Warning, (message), __VA_ARGS__); }
#define DEBUG_LOG_INFO(message, ...) { Debug::Log(Debug::Severity::Info, (message), __VA_ARGS__); }
#define DEBUG_LOG(message, ...) { Debug::Log(Debug::Severity::Debug, (message), __VA_ARGS__); }
#else
#define DEBUG_ASSERT(condition, message, ...)
#define DEBUG_ASSERT_N(condition)
#define DEBUG_LOG_CRITICAL(message, ...)
#define DEBUG_LOG_ERROR(message, ...)
#define DEBUG_LOG_WARNING(message, ...)
#define DEBUG_LOG_INFO(message, ...)
#define DEBUG_LOG(message, ...)
#endif

class Debug
{
public:
    enum class Severity
    {
        Assert,
        Critical,
        Error,
        Warning,
        Info,
        Debug
    };

    static void Assert(bool condition, const char* message, ...);
    static void Log(Severity severity, const char* message, ...);

private:
    const static inline std::string FormatLogMessage(const char* message, const std::string& arg)
    {
        return FormatLogMessage(message, arg.c_str());
    }

    const static inline std::string FormatLogMessage(const char* message, const char* arg)
    {
        size_t bufferLength = 0;
        {
            bufferLength = std::snprintf(nullptr, 0, message, arg);
        }

        char* buffer = DBG_NEW char[bufferLength + 1];
        memset(buffer, 0, bufferLength + 1);

        std::snprintf(buffer, bufferLength + 1, message, arg);
        std::string retValue(buffer);

        delete[] buffer;
        buffer = nullptr;

        return retValue;
    }

    const static inline std::string FormatLogMessage(const char* message, va_list args)
    {
        size_t bufferLength = 0;
        {
            va_list length_check_args;
            va_copy(length_check_args, args);
            bufferLength = std::vsnprintf(nullptr, 0, message, length_check_args);
            va_end(length_check_args);
        }

        char* buffer = DBG_NEW char[bufferLength + 1];
        memset(buffer, 0, bufferLength + 1);

        std::vsnprintf(buffer, bufferLength + 1, message, args);

        std::string retValue(buffer);

        delete[] buffer;
        buffer = nullptr;

        return retValue;
    }

    const static inline std::string FormatLogMessage(const char* message, ...)
    {
        va_list args;
        va_start(args, message);

        std::string retValue = FormatLogMessage(message, args);

        va_end(args);

        return retValue;
    }
};