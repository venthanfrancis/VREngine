#include "AREngine/Core/Assert.hpp"

#include <cstdlib>
#include <iostream>

namespace AREngine::Core
{
    void AssertFailure(const char* expression, const char* file, int line, const char* message)
    {
        std::cerr << "Assertion failed: " << expression << '\n'
                   << "  at " << file << ':' << line << '\n';

        if (message != nullptr && message[0] != '\0')
        {
            std::cerr << "  " << message << '\n';
        }

        std::abort();
    }
}
