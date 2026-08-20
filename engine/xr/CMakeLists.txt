add_library(arengine_xr STATIC
    src/XR.cpp
)
add_library(AREngine::XR ALIAS arengine_xr)

target_include_directories(arengine_xr
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(arengine_xr
    PUBLIC
        AREngine::Core
        AREngine::Platform
        AREngine::Frame
)

target_compile_features(arengine_xr PUBLIC cxx_std_20)

# No OpenXR (or any XR runtime) dependency yet — that lands at M9.
# Frame is linked now because this module will eventually implement
# XRFrameDriver (see docs/ARCHITECTURE.md, "FrameDriver Abstraction").
