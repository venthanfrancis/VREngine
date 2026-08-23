add_library(arengine_scene STATIC
    src/Scene.cpp
)
add_library(AREngine::Scene ALIAS arengine_scene)

target_sources(arengine_scene
    PRIVATE
        include/AREngine/Scene/Scene.hpp
        include/AREngine/Scene/EntityId.hpp
        include/AREngine/Scene/Transform.hpp
        include/AREngine/Scene/Camera.hpp
)

target_include_directories(arengine_scene
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(arengine_scene
    PUBLIC
        AREngine::Core
)

target_compile_features(arengine_scene PUBLIC cxx_std_20)

# Scene must never depend on Rendering, Physics, or XR — it exposes data
# (entities, transforms, hierarchy) that those modules consume through
# interfaces, not the other way around. See docs/ARCHITECTURE.md.
