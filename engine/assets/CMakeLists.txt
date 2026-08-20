add_library(arengine_assets STATIC
    src/Assets.cpp
)
add_library(AREngine::Assets ALIAS arengine_assets)

target_include_directories(arengine_assets
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(arengine_assets
    PUBLIC
        AREngine::Core
        AREngine::Platform
)

target_compile_features(arengine_assets PUBLIC cxx_std_20)
