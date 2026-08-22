if(ARENGINE_ENABLE_VULKAN)
    find_package(Vulkan REQUIRED)
endif()

add_library(arengine_rendering STATIC
    src/Rendering.cpp
    src/NullRenderDevice.cpp
)
add_library(AREngine::Rendering ALIAS arengine_rendering)

target_sources(arengine_rendering
    PRIVATE
        include/AREngine/Rendering/Rendering.hpp
        include/AREngine/Rendering/Handles.hpp
        include/AREngine/Rendering/BufferDesc.hpp
        include/AREngine/Rendering/TextureDesc.hpp
        include/AREngine/Rendering/DrawCommand.hpp
        include/AREngine/Rendering/RenderDevice.hpp
        include/AREngine/Rendering/NullRenderDevice.hpp
)

if(ARENGINE_ENABLE_VULKAN)
    target_sources(arengine_rendering
        PRIVATE
            src/vulkan/VulkanVersion.cpp
            src/vulkan/VulkanResult.cpp
            src/vulkan/VulkanPhysicalDevice.cpp
            src/vulkan/VulkanInstance.cpp
            src/vulkan/VulkanDevice.cpp
            src/vulkan/VulkanQueueFamilies.cpp
            src/vulkan/VulkanSwapchainSupport.cpp
            src/vulkan/VulkanSurface.cpp
            src/vulkan/VulkanSwapchain.cpp
            src/vulkan/VulkanCommandPool.cpp
            src/vulkan/VulkanImageBarrier.cpp
            src/vulkan/VulkanVersion.hpp
            src/vulkan/VulkanResult.hpp
            src/vulkan/VulkanPhysicalDevice.hpp
            src/vulkan/VulkanInstance.hpp
            src/vulkan/VulkanDevice.hpp
            src/vulkan/VulkanQueueFamilies.hpp
            src/vulkan/VulkanSwapchainSupport.hpp
            src/vulkan/VulkanSurface.hpp
            src/vulkan/VulkanSwapchain.hpp
            src/vulkan/VulkanCommandPool.hpp
            src/vulkan/VulkanImageBarrier.hpp
    )
    target_link_libraries(arengine_rendering PRIVATE Vulkan::Vulkan AREngine::Platform)
endif()

target_include_directories(arengine_rendering
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(arengine_rendering
    PUBLIC
        AREngine::Core
)

target_compile_features(arengine_rendering PUBLIC cxx_std_20)

# The generic RHI (Rendering.hpp/RenderDevice.hpp/NullRenderDevice) is
# still NOT linked to Platform or Frame: it stays scoped to GPU
# rendering operations only, independent of any window. See
# docs/ARCHITECTURE.md, "RHI Presentation".
#
# AREngine::Platform IS linked, but only PRIVATE and only inside the
# ARENGINE_ENABLE_VULKAN block above: M8B's Vulkan backend needs
# Platform::NativeWindowHandle to create a VkSurfaceKHR (see
# src/vulkan/VulkanSurface.hpp), but that dependency is isolated to
# this one internal implementation detail of Rendering, not exposed on
# any public Rendering header, and not present at all when Vulkan is
# disabled. See docs/ARCHITECTURE.md, "Platform Dependency Isolation
# (M8B)".
#
# ARENGINE_ENABLE_VULKAN (top-level option, default ON) gates all of
# Vulkan: when OFF, find_package(Vulkan) is never called, none of
# src/vulkan/'s sources are compiled, and arengine_rendering links
# neither Vulkan nor Platform — NullRenderDevice and everything that
# depends on Rendering keep building normally either way. See
# docs/ARCHITECTURE.md, "Making Vulkan Optional".
#
# When ON: Vulkan::Vulkan is linked PRIVATE — Vulkan is an
# implementation detail of this module (src/vulkan/), never named in a
# public Rendering header. find_package(Vulkan) uses CMake's standard
# Vulkan SDK discovery (via the VULKAN_SDK environment variable the SDK
# installer sets) — no vendored headers, no hard-coded machine-specific
# path.
