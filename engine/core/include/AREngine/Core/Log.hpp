#pragma once

#include <string_view>

// AREngine::Core logging.
//
// Minimal, console-only logging with four levels. The public API
// (the Log function and the AR_LOG_* macros) is intentionally small so
// the backend — currently just stdout/stderr — can be replaced later
// without touching call sites.
//
// Logging must not depend on Platform, Rendering, Runtime, or any other
// higher-level module (see docs/ARCHITECTURE.md).

namespace AREngine::Core
{
    enum class LogLevel
    {
        Trace,
        Info,
        Warning,
        Error,
    };

    // Writes one line to the console, prefixed with the level. Not
    // thread-safe yet — Core has no threading story until a later
    // milestone.
    void Log(LogLevel level, std::string_view message);
}

#define AR_LOG_TRACE(message)   ::AREngine::Core::Log(::AREngine::Core::LogLevel::Trace,   message)
#define AR_LOG_INFO(message)    ::AREngine::Core::Log(::AREngine::Core::LogLevel::Info,    message)
#define AR_LOG_WARNING(message) ::AREngine::Core::Log(::AREngine::Core::LogLevel::Warning, message)
#define AR_LOG_ERROR(message)   ::AREngine::Core::Log(::AREngine::Core::LogLevel::Error,   message)
