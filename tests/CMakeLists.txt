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

# Pure-logic tests for XRInteractionState.hpp/.cpp (M10.6): input-to-
# visual-state mapping, tested with synthetic AREngine::Input::*ActionState
# values. Built unconditionally, not gated behind ARENGINE_ENABLE_OPENXR/
# ARENGINE_ENABLE_VULKAN: XRInteractionState depends only on
# AREngine::Input/AREngine::Core::Math - no OpenXR or Vulkan type
# anywhere. See docs/ARCHITECTURE.md, "Where Interaction Logic Lives
# (M10.6)".
add_executable(arengine_xr_interaction_tests
    xr_interaction_tests.cpp
    XRInteractionState.cpp
)
target_link_libraries(arengine_xr_interaction_tests PRIVATE AREngine::Core AREngine::Input)
target_compile_features(arengine_xr_interaction_tests PRIVATE cxx_std_20)
add_test(NAME XRInteractionTests COMMAND arengine_xr_interaction_tests)

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

# Everything OpenXR-specific is gated on ARENGINE_ENABLE_OPENXR (top-
# level option, default OFF) — when OFF, none of this is added: no
# OpenXR headers/loader required at all. Deliberately independent of
# ARENGINE_ENABLE_VULKAN in both directions (its own if-block, not
# nested inside the Vulkan one) — M9A must build and test whether or
# not Vulkan is enabled. See docs/ARCHITECTURE.md, "OpenXR Build Option
# (M9A)".
if(ARENGINE_ENABLE_OPENXR)
    # Pure-logic OpenXR tests: no real OpenXR API calls, so no loader/
    # runtime/headset is required at runtime — see openxr_tests.cpp.
    add_executable(arengine_openxr_tests
        openxr_tests.cpp
    )
    target_link_libraries(arengine_openxr_tests PRIVATE AREngine::Core AREngine::XR OpenXR::openxr_loader)
    target_include_directories(arengine_openxr_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/xr/src)
    target_compile_features(arengine_openxr_tests PRIVATE cxx_std_20)
    add_test(NAME OpenXRTests COMMAND arengine_openxr_tests)

    # Manual-only: requires a real OpenXR loader/runtime (and ideally a
    # real headset) which CI/headless systems lack. Built by CMake but
    # deliberately NOT registered with add_test. Run it directly
    # instead. See docs/ARCHITECTURE.md, "M9A Implementation Notes".
    add_executable(arengine_openxr_demo
        openxr_demo.cpp
    )
    target_link_libraries(arengine_openxr_demo PRIVATE AREngine::Core AREngine::XR OpenXR::openxr_loader)
    target_include_directories(arengine_openxr_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/xr/src)
    target_compile_features(arengine_openxr_demo PRIVATE cxx_std_20)

    # Pure-logic OpenXR session-lifecycle tests (M9D): session-state
    # formatting/decisions, view-configuration selection, reference-
    # space selection. No real OpenXR calls and no Vulkan dependency -
    # see openxr_session_tests.cpp.
    add_executable(arengine_openxr_session_tests
        openxr_session_tests.cpp
    )
    target_link_libraries(arengine_openxr_session_tests PRIVATE AREngine::Core AREngine::XR AREngine::Frame OpenXR::openxr_loader)
    target_include_directories(arengine_openxr_session_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/xr/src)
    target_compile_features(arengine_openxr_session_tests PRIVATE cxx_std_20)
    add_test(NAME OpenXRSessionTests COMMAND arengine_openxr_session_tests)

    # Pure-logic OpenXR view-location tests (M9F): XrPosef/XrFovf ->
    # Frame::ViewInfo conversion, XrViewState validity, and
    # OpenXRProjectionLayer's composition-layer preparation (including
    # its view/sub-image count mismatch handling). No real OpenXR calls
    # and no Vulkan dependency - see openxr_view_tests.cpp.
    add_executable(arengine_openxr_view_tests
        openxr_view_tests.cpp
    )
    target_link_libraries(arengine_openxr_view_tests PRIVATE AREngine::Core AREngine::XR AREngine::Frame OpenXR::openxr_loader)
    target_include_directories(arengine_openxr_view_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/xr/src)
    target_compile_features(arengine_openxr_view_tests PRIVATE cxx_std_20)
    add_test(NAME OpenXRViewTests COMMAND arengine_openxr_view_tests)

    # Pure-logic OpenXR action-state-conversion tests (M10):
    # XrActionStateBoolean/Float/Vector2f/Pose -> AREngine::Input's
    # generic DigitalActionState/AnalogActionState/Vector2ActionState/
    # PoseActionState conversion, including digital press/release
    # transition logic and inactive-state handling. No real OpenXR
    # calls and no Vulkan dependency - see openxr_action_tests.cpp.
    add_executable(arengine_openxr_action_tests
        openxr_action_tests.cpp
    )
    target_link_libraries(arengine_openxr_action_tests PRIVATE AREngine::Core AREngine::XR AREngine::Frame AREngine::Input OpenXR::openxr_loader)
    target_include_directories(arengine_openxr_action_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/xr/src)
    target_compile_features(arengine_openxr_action_tests PRIVATE cxx_std_20)
    add_test(NAME OpenXRActionTests COMMAND arengine_openxr_action_tests)

    # --- M9C: OpenXR/Vulkan integration boundary ---
    #
    # Only built when BOTH ARENGINE_ENABLE_OPENXR and
    # ARENGINE_ENABLE_VULKAN are ON - nested inside the OpenXR block
    # (not the Vulkan block above) so OPENXR=ON/VULKAN=OFF still builds
    # and tests everything else in this block. See
    # docs/ARCHITECTURE.md, "Build Options (M9C)".
    if(ARENGINE_ENABLE_VULKAN)
        find_package(Vulkan REQUIRED)

        # Pure-logic OpenXR/Vulkan tests: no real OpenXR or Vulkan API
        # calls, so no loader/runtime/headset/GPU is required at
        # runtime — see openxr_vulkan_tests.cpp.
        add_executable(arengine_openxr_vulkan_tests
            openxr_vulkan_tests.cpp
        )
        target_link_libraries(arengine_openxr_vulkan_tests PRIVATE AREngine::Core AREngine::XR OpenXR::openxr_loader Vulkan::Vulkan)
        target_include_directories(arengine_openxr_vulkan_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/xr/src)
        target_compile_features(arengine_openxr_vulkan_tests PRIVATE cxx_std_20)
        add_test(NAME OpenXRVulkanTests COMMAND arengine_openxr_vulkan_tests)

        # Manual-only: requires a real OpenXR loader/runtime with
        # XR_KHR_vulkan_enable2 support (and ideally a real or
        # simulated HMD) which CI/headless systems lack. Built by CMake
        # but deliberately NOT registered with add_test. Run it
        # directly instead. See docs/ARCHITECTURE.md, "M9C
        # Implementation Notes".
        add_executable(arengine_openxr_vulkan_demo
            openxr_vulkan_demo.cpp
        )
        target_link_libraries(arengine_openxr_vulkan_demo PRIVATE AREngine::Core AREngine::XR OpenXR::openxr_loader Vulkan::Vulkan)
        target_include_directories(arengine_openxr_vulkan_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/xr/src)
        target_compile_features(arengine_openxr_vulkan_demo PRIVATE cxx_std_20)

        # Manual-only, M9D: requires a real OpenXR loader/runtime with
        # XR_KHR_vulkan_enable2 support (and ideally a real or
        # simulated HMD) which CI/headless systems lack. Built by CMake
        # but deliberately NOT registered with add_test. Run it
        # directly instead. See docs/ARCHITECTURE.md, "M9D
        # Implementation Notes".
        add_executable(arengine_openxr_session_demo
            openxr_session_demo.cpp
        )
        target_link_libraries(arengine_openxr_session_demo PRIVATE AREngine::Core AREngine::XR AREngine::Frame OpenXR::openxr_loader Vulkan::Vulkan)
        target_include_directories(arengine_openxr_session_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/xr/src)
        target_compile_features(arengine_openxr_session_demo PRIVATE cxx_std_20)

        # Manual-only, M9E: requires a real OpenXR loader/runtime with
        # XR_KHR_vulkan_enable2 support (and ideally a real or simulated
        # HMD) which CI/headless systems lack. Built by CMake but
        # deliberately NOT registered with add_test. Run it directly
        # instead. See docs/ARCHITECTURE.md, "M9E Implementation Notes".
        add_executable(arengine_openxr_frame_demo
            openxr_frame_demo.cpp
        )
        target_link_libraries(arengine_openxr_frame_demo PRIVATE AREngine::Core AREngine::XR AREngine::Frame OpenXR::openxr_loader Vulkan::Vulkan)
        target_include_directories(arengine_openxr_frame_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/xr/src)
        target_compile_features(arengine_openxr_frame_demo PRIVATE cxx_std_20)

        # Manual-only, M9G: requires a real OpenXR loader/runtime with
        # XR_KHR_vulkan_enable2 support (and ideally a real or simulated
        # HMD) which CI/headless systems lack. Built by CMake but
        # deliberately NOT registered with add_test. Run it directly
        # instead. Reaches into BOTH engine/xr's and engine/rendering's
        # private src/vulkan/ implementation directly (the "M9G demo may
        # coordinate these systems directly" the milestone brief grants)
        # - engine/xr itself gains no new dependency on engine/rendering
        # from this, only this leaf demo target does. See
        # docs/ARCHITECTURE.md, "M9G Implementation Notes".
        add_executable(arengine_openxr_cube_demo
            openxr_cube_demo.cpp
            OpenXRVulkanViewTarget.cpp
        )
        target_link_libraries(arengine_openxr_cube_demo PRIVATE AREngine::Core AREngine::XR AREngine::Frame AREngine::Rendering AREngine::Scene OpenXR::openxr_loader Vulkan::Vulkan)
        target_include_directories(arengine_openxr_cube_demo PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../engine/xr/src
            ${CMAKE_CURRENT_SOURCE_DIR}/../engine/rendering/src)
        target_compile_features(arengine_openxr_cube_demo PRIVATE cxx_std_20)

        # Manual-only, M10: requires a real OpenXR loader/runtime with
        # XR_KHR_vulkan_enable2 support (this codebase's XrSession
        # creation needs the Vulkan graphics binding, even though this
        # demo renders nothing itself - see the demo's own header
        # comment) which CI/headless systems lack. Built by CMake but
        # deliberately NOT registered with add_test. Run it directly
        # instead. See docs/ARCHITECTURE.md, "M10 Implementation Notes".
        add_executable(arengine_openxr_input_demo
            openxr_input_demo.cpp
        )
        target_link_libraries(arengine_openxr_input_demo PRIVATE AREngine::Core AREngine::XR AREngine::Frame AREngine::Input OpenXR::openxr_loader Vulkan::Vulkan)
        target_include_directories(arengine_openxr_input_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../engine/xr/src)
        target_compile_features(arengine_openxr_input_demo PRIVATE cxx_std_20)

        # Manual-only, M10.5: AREngine's first INTEGRATED XR demo -
        # combines the render path (M9G/M9H) and the action/input path
        # (M10) in one session/frame loop. Requires a real OpenXR
        # loader/runtime with XR_KHR_vulkan_enable2 support which
        # CI/headless systems lack. Built by CMake but deliberately NOT
        # registered with add_test. Run it directly instead. Reaches
        # into BOTH engine/xr's and engine/rendering's private
        # src/vulkan/ implementation directly, same as
        # openxr_cube_demo.cpp - engine/xr itself gains no new
        # dependency on engine/rendering from this, only this leaf demo
        # target does. See docs/ARCHITECTURE.md, "M10.5 Implementation
        # Notes".
        add_executable(arengine_xr_demo
            xr_demo.cpp
            OpenXRVulkanViewTarget.cpp
            XRInteractionState.cpp
        )
        target_link_libraries(arengine_xr_demo PRIVATE AREngine::Core AREngine::XR AREngine::Frame AREngine::Rendering AREngine::Scene AREngine::Input OpenXR::openxr_loader Vulkan::Vulkan)
        target_include_directories(arengine_xr_demo PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../engine/xr/src
            ${CMAKE_CURRENT_SOURCE_DIR}/../engine/rendering/src)
        target_compile_features(arengine_xr_demo PRIVATE cxx_std_20)
    endif()
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
