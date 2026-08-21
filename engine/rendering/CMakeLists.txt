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
            src/vulkan/VulkanVersion.hpp
            src/vulkan/VulkanResult.hpp
            src/vulkan/VulkanPhysicalDevice.hpp
            src/vulkan/VulkanInstance.hpp
            src/vulkan/VulkanDevice.hpp
    )
    target_link_libraries(arengine_rendering PRIVATE Vulkan::Vulkan)
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

# Deliberately not linked to Platform or Frame: the RHI is scoped to GPU
# rendering operations only. Presentation/frame submission is a separate
# concern owned by Frame + Runtime (see docs/ARCHITECTURE.md, "RHI
# Presentation"). A Platform dependency (e.g. for a window surface) is
# expected to be added once a swapchain milestone needs one.
#
# ARENGINE_ENABLE_VULKAN (top-level option, default ON) gates all of
# Vulkan: when OFF, find_package(Vulkan) is never called, none of
# src/vulkan/'s sources are compiled, and arengine_rendering links no
# Vulkan library at all — NullRenderDevice and everything that depends
# on Rendering keep building normally either way. See
# docs/ARCHITECTURE.md, "Making Vulkan Optional".
#
# When ON: Vulkan::Vulkan is linked PRIVATE — Vulkan is an
# implementation detail of this module (src/vulkan/), never named in a
# public Rendering header. find_package(Vulkan) uses CMake's standard
# Vulkan SDK discovery (via the VULKAN_SDK environment variable the SDK
# installer sets) — no vendored headers, no hard-coded machine-specific
# path.
