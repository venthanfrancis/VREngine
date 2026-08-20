add_library(arengine_core STATIC
    src/Core.cpp
)
add_library(AREngine::Core ALIAS arengine_core)

target_include_directories(arengine_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(arengine_core PUBLIC cxx_std_20)

# Core depends on nothing else in the engine (see docs/ARCHITECTURE.md).
