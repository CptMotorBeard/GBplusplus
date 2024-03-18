#include "stdafx.h"
#include "header/Debug.h"

void Debug::Assert(bool condition, const char* message, ...)
{
#if defined(CT_DEBUG) || defined(CT_OPTDEBUG)
    if (!condition)
    {
        va_list args;
        va_start(args, message);

        Log(Severity::Assert, message, args);
        __debugbreak();

        va_end(args);
    }
#endif // CT_DEBUG || CT_OPTDEBUG
}

void Debug::Log(Severity severity, const char* message, ...)
{
#if defined(CT_DEBUG) || defined(CT_OPTDEBUG)
    va_list args;
    va_start(args, message);

    std::string formattedMessage = FormatLogMessage(message, args);

    switch (severity)
    {
    case Severity::Assert:
        std::cerr << "[ASSERT] ";
        break;
    case Severity::Critical:
        std::cerr << "[CRITICAL] ";
        break;
    case Severity::Error:
        std::cerr << "[ERROR] ";
        break;
    case Severity::Warning:
        std::cerr << "[Warning] ";
        break;
    case Severity::Info:
        std::cerr << "[Info] ";
        break;
    case Severity::Debug:
        std::cerr << "[Debug] ";
        break;
    }

    std::cerr << formattedMessage << std::endl;

    va_end(args);
#endif // CT_DEBUG || CT_OPTDEBUG
}