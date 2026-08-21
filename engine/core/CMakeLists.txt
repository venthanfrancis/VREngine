add_library(arengine_core STATIC
    src/Core.cpp
    src/Log.cpp
    src/Assert.cpp
)
add_library(AREngine::Core ALIAS arengine_core)

# Header-only sources (Event, Math) are listed here too, purely so they
# show up in IDE project views — they don't need their own .cpp files.
target_sources(arengine_core
    PRIVATE
        include/AREngine/Core/Core.hpp
        include/AREngine/Core/Log.hpp
        include/AREngine/Core/Assert.hpp
        include/AREngine/Core/Event.hpp
        include/AREngine/Core/KeyCode.hpp
        include/AREngine/Core/MouseButton.hpp
        include/AREngine/Core/Math/Math.hpp
        include/AREngine/Core/Math/MathUtil.hpp
        include/AREngine/Core/Math/Vec2.hpp
        include/AREngine/Core/Math/Vec3.hpp
        include/AREngine/Core/Math/Vec4.hpp
        include/AREngine/Core/Math/Quaternion.hpp
        include/AREngine/Core/Math/Mat4.hpp
)

target_include_directories(arengine_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(arengine_core PUBLIC cxx_std_20)

# Core depends on nothing else in the engine (see docs/ARCHITECTURE.md).
