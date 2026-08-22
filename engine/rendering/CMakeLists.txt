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
            src/vulkan/VulkanShaderModule.cpp
            src/vulkan/VulkanRenderPass.cpp
            src/vulkan/VulkanFramebuffers.cpp
            src/vulkan/VulkanGraphicsPipeline.cpp
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
            src/vulkan/VulkanShaderModule.hpp
            src/vulkan/VulkanRenderPass.hpp
            src/vulkan/VulkanFramebuffers.hpp
            src/vulkan/VulkanGraphicsPipeline.hpp
    )
    target_link_libraries(arengine_rendering PRIVATE Vulkan::Vulkan AREngine::Platform)

    # --- M8C: compile GLSL shaders to SPIR-V at build time ---
    #
    # glslc ships with the Vulkan SDK. Prefer whatever CMake's FindVulkan
    # module already discovered (Vulkan_GLSLC_EXECUTABLE - populated
    # automatically by recent CMake versions alongside Vulkan_LIBRARY/
    # Vulkan_INCLUDE_DIR); fall back to find_program() using the same
    # VULKAN_SDK environment variable the SDK installer sets, for older
    # CMake versions where FindVulkan doesn't locate SDK tools. Either
    # way, no hard-coded SDK version or path.
    if(Vulkan_GLSLC_EXECUTABLE)
        set(ARENGINE_GLSLC_EXECUTABLE ${Vulkan_GLSLC_EXECUTABLE})
    else()
        find_program(ARENGINE_GLSLC_EXECUTABLE
            NAMES glslc
            HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
        )
    endif()
    if(NOT ARENGINE_GLSLC_EXECUTABLE)
        message(FATAL_ERROR
            "ARENGINE_ENABLE_VULKAN requires glslc (part of the Vulkan SDK) "
            "to compile M8C's shaders to SPIR-V, but it could not be found. "
            "Ensure the Vulkan SDK is installed and VULKAN_SDK is set."
        )
    endif()

    set(ARENGINE_SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan/shaders)
    set(ARENGINE_SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders)

    set(ARENGINE_SHADER_SOURCES
        ${ARENGINE_SHADER_SOURCE_DIR}/triangle.vert
        ${ARENGINE_SHADER_SOURCE_DIR}/triangle.frag
    )
    set(ARENGINE_SHADER_OUTPUTS "")
    foreach(shader_source ${ARENGINE_SHADER_SOURCES})
        get_filename_component(shader_name ${shader_source} NAME)
        set(shader_output ${ARENGINE_SHADER_OUTPUT_DIR}/${shader_name}.spv)
        add_custom_command(
            OUTPUT ${shader_output}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${ARENGINE_SHADER_OUTPUT_DIR}
            COMMAND ${ARENGINE_GLSLC_EXECUTABLE} ${shader_source} -o ${shader_output}
            DEPENDS ${shader_source}
            COMMENT "Compiling shader ${shader_name} to SPIR-V"
            VERBATIM
        )
        list(APPEND ARENGINE_SHADER_OUTPUTS ${shader_output})
    endforeach()

    add_custom_target(arengine_shaders DEPENDS ${ARENGINE_SHADER_OUTPUTS})
    add_dependencies(arengine_rendering arengine_shaders)

    # Only VulkanGraphicsPipeline.cpp reads this - it's how a Vulkan-
    # private implementation file finds the SPIR-V CMake just built,
    # without ever exposing a shader path (or VkShaderModule) on any
    # public Rendering header. See docs/ARCHITECTURE.md, "SPIR-V Build
    # Path (M8C)".
    target_compile_definitions(arengine_rendering PRIVATE ARENGINE_SHADER_DIR="${ARENGINE_SHADER_OUTPUT_DIR}")
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
