# No third-party test framework is wired in yet (see docs/ROADMAP.md —
# that decision is deferred). Each test here is a plain executable that
# returns 0 on success; CTest just checks the exit code.

add_executable(arengine_core_tests
    core_tests.cpp
)
target_link_libraries(arengine_core_tests PRIVATE AREngine::Core)
target_compile_features(arengine_core_tests PRIVATE cxx_std_20)
add_test(NAME CoreTests COMMAND arengine_core_tests)

add_executable(arengine_frame_tests
    frame_tests.cpp
)
target_link_libraries(arengine_frame_tests PRIVATE AREngine::Core AREngine::Frame)
target_compile_features(arengine_frame_tests PRIVATE cxx_std_20)
add_test(NAME FrameTests COMMAND arengine_frame_tests)
