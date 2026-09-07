add_library(arengine_physics STATIC
    src/FixedTimestepAccumulator.cpp
)
add_library(AREngine::Physics ALIAS arengine_physics)

target_sources(arengine_physics
    PRIVATE
        include/AREngine/Physics/Physics.hpp
        include/AREngine/Physics/PhysicsBodyId.hpp
        include/AREngine/Physics/PhysicsPose.hpp
        include/AREngine/Physics/ColliderDesc.hpp
        include/AREngine/Physics/RigidBodyDesc.hpp
        include/AREngine/Physics/FixedTimestepAccumulator.hpp
)

target_include_directories(arengine_physics
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(arengine_physics
    PUBLIC
        AREngine::Core
)

target_compile_features(arengine_physics PUBLIC cxx_std_20)

# ARENGINE_ENABLE_PHYSICS (top-level option, default OFF, mirroring
# ARENGINE_ENABLE_OPENXR's exact wording/default pattern) gates
# everything that actually touches the Jolt Physics backend: when OFF,
# no find_package/FetchContent call happens, none of src/vulkan-style
# src/jolt/ sources compile, and arengine_physics still builds (with
# only the unconditional, backend-neutral types above) - the same
# "generic types always compiled, only the third-party-touching
# implementation gated" shape engine/rendering already established for
# ARENGINE_ENABLE_VULKAN. See docs/ARCHITECTURE.md, "M18 - Physics
# Foundation".
if(ARENGINE_ENABLE_PHYSICS)
    include(FetchContent)

    # Suppress Jolt's own extra build targets (tests/samples/viewer/
    # perf-test/hello-world) - AREngine only needs the Jolt library
    # itself. Mirrors the exact BUILD_TESTS=OFF-style suppression
    # already used for OpenXR-SDK's own FetchContent block
    # (engine/xr/CMakeLists.txt).
    set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
    set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
    set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
    set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)

    # Jolt's own Build/CMakeLists.txt defaults to the STATIC MSVC
    # runtime (/MT) when built standalone, while AREngine (and the
    # Vulkan SDK import libs it already links) build with the ordinary
    # dynamic CRT (/MD) - left unset, this produces a real
    # LNK2038 "RuntimeLibrary mismatch" link error the moment anything
    # links both AREngine::Core-family static libs and Jolt. Force it
    # off to match AREngine's own CRT choice.
    set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)

    # Jolt's own top-level CMakeLists.txt lives at Build/CMakeLists.txt,
    # not the repository root - SOURCE_SUBDIR points FetchContent at it
    # (a real, supported CMake mechanism since 3.18; this project
    # requires 3.21). Pinned to the v5.6.0 tagged release, not a
    # floating branch. Vendored via FetchContent rather than a direct
    # single-header vendor (like stb_image.h/tinyobjloader): Jolt is a
    # full multi-file CMake project, matching the OpenXR-SDK precedent
    # (fetched, gated behind a default-OFF flag), not the single-header
    # precedent. See docs/ROADMAP.md's "No third-party dependencies
    # until a milestone explicitly calls for one" - M18 is that
    # milestone for Physics.
    FetchContent_Declare(
        Jolt
        GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
        GIT_TAG v5.6.0
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR Build
    )
    FetchContent_MakeAvailable(Jolt)

    target_sources(arengine_physics
        PRIVATE
            include/AREngine/Physics/PhysicsWorld.hpp
            include/AREngine/Physics/PhysicsSceneBridge.hpp
            src/PhysicsWorld.cpp
            src/PhysicsSceneBridge.cpp
            src/jolt/JoltConversions.hpp
            src/jolt/JoltLayers.hpp
            src/jolt/JoltLayers.cpp
    )

    # PRIVATE: Jolt is an implementation detail of src/PhysicsWorld.cpp/
    # src/jolt/ only, never part of this library's public interface -
    # same discipline as Vulkan::Vulkan in engine/rendering.
    #
    # AREngine::Scene is also PRIVATE here - the one pre-authorized
    # Physics -> Scene edge (see docs/ARCHITECTURE.md), needed only by
    # PhysicsSceneBridge.cpp, not by anything in this library's public
    # headers beyond naming Scene::Scene/EntityId in
    # PhysicsSceneBridge.hpp's own signatures.
    target_link_libraries(arengine_physics
        PRIVATE
            Jolt
            AREngine::Scene
    )
endif()
