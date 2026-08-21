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

add_executable(arengine_rendering_tests
    rendering_tests.cpp
)
target_link_libraries(arengine_rendering_tests PRIVATE AREngine::Core AREngine::Rendering)
target_compile_features(arengine_rendering_tests PRIVATE cxx_std_20)
add_test(NAME RenderingTests COMMAND arengine_rendering_tests)

add_executable(arengine_scene_tests
    scene_tests.cpp
)
target_link_libraries(arengine_scene_tests PRIVATE AREngine::Core AREngine::Scene)
target_compile_features(arengine_scene_tests PRIVATE cxx_std_20)
add_test(NAME SceneTests COMMAND arengine_scene_tests)

add_executable(arengine_asset_tests
    asset_tests.cpp
)
target_link_libraries(arengine_asset_tests PRIVATE AREngine::Core AREngine::Assets)
target_compile_features(arengine_asset_tests PRIVATE cxx_std_20)
# Points at the tiny fixture files in tests/data/assets/ via a path
# CMake computes from this source tree — not a machine-specific
# hard-coded path, so this works on any checkout.
target_compile_definitions(arengine_asset_tests PRIVATE
    AR_TEST_ASSETS_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/data/assets"
)
add_test(NAME AssetTests COMMAND arengine_asset_tests)

add_executable(arengine_input_tests
    input_tests.cpp
)
target_link_libraries(arengine_input_tests PRIVATE AREngine::Core AREngine::Platform AREngine::Input)
target_compile_features(arengine_input_tests PRIVATE cxx_std_20)
add_test(NAME InputTests COMMAND arengine_input_tests)

# Manual-only: proves M2's Window works end-to-end with real user
# interaction (opening a window, closing it via the close button). Built
# by CMake but deliberately NOT registered with add_test — CTest must
# never be left waiting on a human. Run it directly instead.
add_executable(arengine_platform_window_demo
    platform_window_demo.cpp
)
target_link_libraries(arengine_platform_window_demo PRIVATE AREngine::Core AREngine::Platform)
target_compile_features(arengine_platform_window_demo PRIVATE cxx_std_20)
