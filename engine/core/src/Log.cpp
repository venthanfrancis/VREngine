#include "AREngine/Core/Log.hpp"

#include <iostream>

namespace AREngine::Core
{
    namespace
    {
        std::string_view LevelLabel(LogLevel level)
        {
            switch (level)
            {
                case LogLevel::Trace:   return "TRACE";
                case LogLevel::Info:    return "INFO";
                case LogLevel::Warning: return "WARN";
                case LogLevel::Error:   return "ERROR";
            }
            return "UNKNOWN";
        }
    }

    void Log(LogLevel level, std::string_view message)
    {
        std::ostream& stream = (level == LogLevel::Warning || level == LogLevel::Error) ? std::cerr : std::cout;
        stream << '[' << LevelLabel(level) << "] " << message << '\n';
    }
}
