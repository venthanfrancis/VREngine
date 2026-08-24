add_library(arengine_xr STATIC
    src/XR.cpp
)
add_library(AREngine::XR ALIAS arengine_xr)

target_sources(arengine_xr
    PRIVATE
        include/AREngine/XR/XR.hpp
)

# ARENGINE_ENABLE_OPENXR (top-level option, default OFF) gates all of
# OpenXR: when OFF, no OpenXR headers/loader are required at all, and
# arengine_xr is just the M0-era placeholder it always was. See
# docs/ARCHITECTURE.md, "OpenXR Build Option (M9A)".
if(ARENGINE_ENABLE_OPENXR)
    # --- M9A: acquire the OpenXR loader + headers ---
    #
    # Prefer a system/package-manager-provided OpenXR SDK first (e.g.
    # one installed via vcpkg, which defines an `OpenXR` CMake config
    # package with an `OpenXR::openxr_loader` target) - this is the
    # "use CMake package discovery if available" path the brief asked
    # for. Unlike Vulkan, there is no widely standardized system-wide
    # "OpenXR SDK" installer on Windows (no VULKAN_SDK-style environment
    # variable convention to rely on), so when no such package is found,
    # this falls back to fetching the official Khronos OpenXR-SDK
    # release (pre-generated headers + loader source - nothing is
    # copied into this repository; CMake downloads and builds it, the
    # same way it would build any other subdirectory) via FetchContent,
    # pinned to an exact tagged release for reproducibility. See
    # docs/ARCHITECTURE.md, "OpenXR Loader / SDK Acquisition (M9A)".
    find_package(OpenXR CONFIG QUIET)

    if(NOT TARGET OpenXR::openxr_loader)
        include(FetchContent)

        # Keep the fetched build minimal: only the loader library
        # itself. No API layers, no OpenXR-SDK's own sample/test/
        # conformance suites - none of that is needed to prove
        # instance/system bring-up, and skipping it avoids pulling in
        # Catch2 and the conformance CLI for a dependency this project
        # only needs one library from.
        set(BUILD_LOADER ON CACHE BOOL "" FORCE)
        set(BUILD_API_LAYERS OFF CACHE BOOL "" FORCE)
        set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(BUILD_CONFORMANCE_TESTS OFF CACHE BOOL "" FORCE)

        FetchContent_Declare(
            OpenXRSDK
            GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
            GIT_TAG release-1.1.62
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(OpenXRSDK)
    endif()

    target_sources(arengine_xr
        PRIVATE
            src/openxr/OpenXRVersion.hpp
            src/openxr/OpenXRVersion.cpp
            src/openxr/OpenXRResult.hpp
            src/openxr/OpenXRResult.cpp
            src/openxr/OpenXRInstance.hpp
            src/openxr/OpenXRInstance.cpp
            src/openxr/OpenXRSystem.hpp
            src/openxr/OpenXRSystem.cpp
            src/openxr/OpenXRSessionState.hpp
            src/openxr/OpenXRSessionState.cpp
            src/openxr/OpenXRViewConfiguration.hpp
            src/openxr/OpenXRViewConfiguration.cpp
            src/openxr/OpenXRReferenceSpace.hpp
            src/openxr/OpenXRReferenceSpace.cpp
            # M9E: environment blend mode selection and XrTime
            # conversion are pure OpenXR concepts, no Vulkan dependency
            # - see docs/ARCHITECTURE.md, "Environment Blend Mode
            # Selection (M9E)".
            src/openxr/OpenXREnvironmentBlendMode.hpp
            src/openxr/OpenXREnvironmentBlendMode.cpp
            src/openxr/OpenXRFrameTiming.hpp
    )

    # Private: OpenXR is an implementation detail of this module's
    # src/openxr/ (never named in XR.hpp or any other public XR
    # header), same discipline Rendering already applies to Vulkan.
    target_link_libraries(arengine_xr PRIVATE OpenXR::openxr_loader)

    # --- M9C: OpenXR/Vulkan integration boundary ---
    #
    # Only compiled when BOTH capabilities are enabled - this is the
    # one place in the XR module that needs Vulkan at all, and only for
    # this specific, narrow purpose (constructing the Vulkan objects a
    # future graphics XrSession's binding will need, via OpenXR's own
    # XR_KHR_vulkan_enable2 creation functions - see
    # docs/ARCHITECTURE.md, "XR-Vulkan Integration Placement (M9C)").
    # Guarded independently of the OpenXR-only sources above so
    # ARENGINE_ENABLE_OPENXR=ON with ARENGINE_ENABLE_VULKAN=OFF still
    # builds arengine_xr - just without this file.
    if(ARENGINE_ENABLE_VULKAN)
        find_package(Vulkan REQUIRED)

        target_sources(arengine_xr
            PRIVATE
                src/openxr/OpenXRVulkanRequirements.hpp
                src/openxr/OpenXRVulkanRequirements.cpp
                src/openxr/OpenXRVulkanGraphicsBinding.hpp
                src/openxr/OpenXRVulkanGraphicsBinding.cpp
                # M9D: XrSession creation itself needs
                # XrGraphicsBindingVulkan2KHR - the one part of session
                # lifecycle that is genuinely Vulkan-coupled. See
                # docs/ARCHITECTURE.md, "XrSession Ownership (M9D)".
                src/openxr/OpenXRSession.hpp
                src/openxr/OpenXRSession.cpp
                # M9E: XR swapchains + the VkImages they expose - the
                # other genuinely Vulkan-coupled piece of the frame
                # lifecycle. See docs/ARCHITECTURE.md, "XR Swapchain
                # Topology (M9E)".
                src/openxr/OpenXRSwapchain.hpp
                src/openxr/OpenXRSwapchain.cpp
        )
        # Private, same as OpenXR::openxr_loader above - Vulkan is an
        # implementation detail of this one integration boundary, never
        # named in any public XR header.
        target_link_libraries(arengine_xr PRIVATE Vulkan::Vulkan)
    endif()
endif()

target_include_directories(arengine_xr
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# M9A trims this down to Core only. Platform and Frame were both linked
# here since the M0 stub, in anticipation of needs that hadn't arrived
# yet; M9A is the first real evidence of what XR actually needs right
# now, and it is neither of those - no window/native-handle access
# (Platform) and no FrameDriver implementation yet (Frame, deferred
# until a real XRFrameDriver is built - see docs/ARCHITECTURE.md,
# "Dependency Trim (M9A)"). Both can come back the moment a later
# milestone gives a genuine reason.
target_link_libraries(arengine_xr
    PUBLIC
        AREngine::Core
)

target_compile_features(arengine_xr PUBLIC cxx_std_20)
