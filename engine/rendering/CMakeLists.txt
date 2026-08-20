add_library(arengine_rendering STATIC
    src/Rendering.cpp
)
add_library(AREngine::Rendering ALIAS arengine_rendering)

target_include_directories(arengine_rendering
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(arengine_rendering
    PUBLIC
        AREngine::Core
)

target_compile_features(arengine_rendering PUBLIC cxx_std_20)

# Deliberately not linked to Platform or Frame yet: the RHI is scoped to
# GPU rendering operations only. Presentation/frame submission is a
# separate concern owned by Frame + Runtime (see docs/ARCHITECTURE.md,
# "RHI Presentation"). A Platform dependency (e.g. for a window surface)
# is expected to be added when the Vulkan backend lands (M8), once real
# requirements are known.
