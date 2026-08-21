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

add_executable(arengine_platform_tests
    platform_tests.cpp
)
target_link_libraries(arengine_platform_tests PRIVATE AREngine::Core AREngine::Platform)
target_compile_features(arengine_platform_tests PRIVATE cxx_std_20)
add_test(NAME PlatformTests COMMAND arengine_platform_tests)

add_executable(arengine_runtime_tests
    runtime_tests.cpp
)
target_link_libraries(arengine_runtime_tests PRIVATE AREngine::Core AREngine::Platform AREngine::Runtime)
target_compile_features(arengine_runtime_tests PRIVATE cxx_std_20)
add_test(NAME RuntimeTests COMMAND arengine_runtime_tests)

# Manual-only: proves M2's Window works end-to-end with real user
# interaction (opening a window, closing it via the close button). Built
# by CMake but deliberately NOT registered with add_test — CTest must
# never be left waiting on a human. Run it directly instead.
add_executable(arengine_platform_window_demo
    platform_window_demo.cpp
)
target_link_libraries(arengine_platform_window_demo PRIVATE AREngine::Core AREngine::Platform)
target_compile_features(arengine_platform_window_demo PRIVATE cxx_std_20)
