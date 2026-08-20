add_library(arengine_input STATIC
    src/Input.cpp
)
add_library(AREngine::Input ALIAS arengine_input)

target_include_directories(arengine_input
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(arengine_input
    PUBLIC
        AREngine::Core
        AREngine::Platform
)

target_compile_features(arengine_input PUBLIC cxx_std_20)
