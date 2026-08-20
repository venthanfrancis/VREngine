# No third-party test framework is wired in yet (see docs/ROADMAP.md — that
# decision is deferred). This smoke test only proves that CTest, a test
# executable, and a link against an engine module all work together.

add_executable(arengine_smoke_test
    smoke_test.cpp
)

target_link_libraries(arengine_smoke_test
    PRIVATE
        AREngine::Core
)

target_compile_features(arengine_smoke_test PRIVATE cxx_std_20)

add_test(
    NAME SmokeTest
    COMMAND arengine_smoke_test
)
