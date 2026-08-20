add_library(arengine_platform STATIC
    src/Platform.cpp
)
add_library(AREngine::Platform ALIAS arengine_platform)

target_include_directories(arengine_platform
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(arengine_platform
    PUBLIC
        AREngine::Core
)

target_compile_features(arengine_platform PUBLIC cxx_std_20)

# OS-specific implementation (Windows today; Android/Linux later) lives
# entirely inside this module's src/ — nothing above Platform should ever
# call an OS API directly.
