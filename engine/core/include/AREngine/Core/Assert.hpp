#pragma once

// AREngine::Core assertions.
//
// A lightweight assertion facility: on failure, prints the failed
// expression, source file, line number, and an optional message, then
// terminates the program. This is not a crash-reporting system — just a
// fast way to catch broken invariants during development.
//
// Enabled only in debug builds (disabled whenever NDEBUG is defined, the
// same flag CMake sets for Release-style configurations).

namespace AREngine::Core
{
    // Called when an assertion fails. Prints details and terminates the
    // program. Not intended to be called directly — use AR_ASSERT or
    // AR_ASSERT_MSG instead.
    [[noreturn]] void AssertFailure(const char* expression,
                                     const char* file,
                                     int line,
                                     const char* message);
}

#if !defined(NDEBUG)

#define AR_ASSERT(expression)                                                \
    do                                                                       \
    {                                                                        \
        if (!(expression))                                                   \
        {                                                                    \
            ::AREngine::Core::AssertFailure(#expression, __FILE__, __LINE__, nullptr); \
        }                                                                    \
    } while (false)

#define AR_ASSERT_MSG(expression, message)                                   \
    do                                                                       \
    {                                                                        \
        if (!(expression))                                                   \
        {                                                                    \
            ::AREngine::Core::AssertFailure(#expression, __FILE__, __LINE__, message); \
        }                                                                    \
    } while (false)

#else

// Compiled out entirely, but (void)sizeof(expression) still references
// the expression in an unevaluated context, so variables only used
// inside an assert don't trigger "unused variable" warnings in release.
#define AR_ASSERT(expression) do { (void)sizeof(expression); } while (false)
#define AR_ASSERT_MSG(expression, message) do { (void)sizeof(expression); } while (false)

#endif
