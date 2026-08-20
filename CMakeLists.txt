cmake_minimum_required(VERSION 3.21)

project(AREngine
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep build output in predictable locations regardless of generator.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(ARENGINE_BUILD_EDITOR  "Build the AREngine editor application"       ON)
option(ARENGINE_BUILD_SANDBOX "Build the AREngine sandbox test application" ON)
option(ARENGINE_BUILD_TESTS   "Build AREngine unit tests"                   ON)

# Engine modules (core, frame, platform, rendering, scene, assets, input, xr).
add_subdirectory(engine)

# Runtime: the glue layer that wires engine modules together.
add_subdirectory(runtime)

if(ARENGINE_BUILD_EDITOR)
    add_subdirectory(editor)
endif()

if(ARENGINE_BUILD_SANDBOX)
    add_subdirectory(sandbox)
endif()

if(ARENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
