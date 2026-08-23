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

# Pure-logic tests for AREngine::Rendering's generic mesh data
# (MeshData.hpp/ProceduralMesh.hpp) — M8H. Built unconditionally, not
# gated behind ARENGINE_ENABLE_VULKAN: none of this depends on Vulkan.
# See docs/ARCHITECTURE.md, "CPU Mesh Data Placement (M8H)".
add_executable(arengine_mesh_tests
    mesh_tests.cpp
)
target_link_libraries(arengine_mesh_tests PRIVATE AREngine::Core AREngine::Rendering)
target_compile_features(arengine_mesh_tests PRIVATE cxx_std_20)
add_test(NAME MeshTests COMMAND arengine_mesh_tests)

# Pure-logic tests for ARDemo::DemoCameraController (DemoCameraController.hpp)
# — M8G's manual-Vulkan-demo camera controller. Built unconditionally,
# not gated behind ARENGINE_ENABLE_VULKAN: the controller itself has no
# Vulkan dependency, only Core/Scene/Input, so there is no reason its
# tests should require a Vulkan-capable machine. See
# docs/ARCHITECTURE.md, "Temporary Demo-Controller Decisions (M8G)".
add_executable(arengine_demo_camera_controller_tests
    demo_camera_controller_tests.cpp
)
target_link_libraries(arengine_demo_camera_controller_tests PRIVATE AREngine::Core AREngine::Platform AREngine::Scene AREngine::Input)
target_compile_features(arengine_demo_camera_controller_tests PRIVATE cxx_std_20)
add_test(NAME DemoCameraControllerTests COMMAND arengine_demo_camera_controller_tests)

# Everything Vulkan-specific is gated on ARENGINE_ENABLE_VULKAN (top-
# level option, default ON) — when OFF, none of this is added: no
# find_package(Vulkan) call, no Vulkan targets at all. See
# docs/ARCHITECTURE.md, "Making Vulkan Optional".
if(ARENGINE_ENABLE_VULKAN)
    # find_package(Vulkan) is cached by CMake, so calling it again here
    # (engine/rendering/CMakeLists.txt already called it) is cheap and
    # just confirms the Vulkan::Vulkan target used below is available in
    # this directory's scope too.
    find_package(Vulkan REQUIRED)

    # Pure-logic Vulkan tests: no real Vulkan API calls, so no GPU is
    # required at runtime (see vulkan_tests.cpp for the one caveat this
    # doesn't eliminate — the loader DLL must still be resolvable at
    # process load time, since this links the whole Rendering library).
    add_executable(arengine_vulkan_tests
        vulkan_tests.cpp
    )
    target_link_libraries(arengine_vulkan_tests PRIVATE AREngine::Core AREngine::Rendering Vulkan::Vulkan)
    target_include_directories(arengine_vulkan_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/rendering/src)
    target_compile_features(arengine_vulkan_tests PRIVATE cxx_std_20)
    add_test(NAME VulkanTests COMMAND arengine_vulkan_tests)

    # Manual-only: requires a real Vulkan-capable GPU and driver, which
    # CI/headless systems may lack. Built by CMake but deliberately NOT
    # registered with add_test. Run it directly instead.
    add_executable(arengine_vulkan_demo
        vulkan_demo.cpp
    )
    target_link_libraries(arengine_vulkan_demo PRIVATE AREngine::Core AREngine::Rendering Vulkan::Vulkan)
    target_include_directories(arengine_vulkan_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/rendering/src)
    target_compile_features(arengine_vulkan_demo PRIVATE cxx_std_20)

    # Manual-only, M8B: requires a real Vulkan-capable GPU/driver and
    # opens a real window - CTest must never wait on a human or a GPU.
    # Built by CMake but deliberately NOT registered with add_test. Run
    # it directly instead. See docs/ARCHITECTURE.md, "M8B Implementation
    # Notes".
    add_executable(arengine_vulkan_present_demo
        vulkan_present_demo.cpp
    )
    target_link_libraries(arengine_vulkan_present_demo PRIVATE AREngine::Core AREngine::Platform AREngine::Scene AREngine::Input AREngine::Rendering Vulkan::Vulkan)
    target_include_directories(arengine_vulkan_present_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/rendering/src)
    target_compile_features(arengine_vulkan_present_demo PRIVATE cxx_std_20)
endif()

# Manual-only: proves M2's Window works end-to-end with real user
# interaction (opening a window, closing it via the close button). Built
# by CMake but deliberately NOT registered with add_test — CTest must
# never be left waiting on a human. Run it directly instead.
add_executable(arengine_platform_window_demo
    platform_window_demo.cpp
)
target_link_libraries(arengine_platform_window_demo PRIVATE AREngine::Core AREngine::Platform)
target_compile_features(arengine_platform_window_demo PRIVATE cxx_std_20)
