add_library(arengine_runtime STATIC
    src/Runtime.cpp
    src/DesktopFrameDriver.cpp
)
add_library(AREngine::Runtime ALIAS arengine_runtime)

target_sources(arengine_runtime
    PRIVATE
        include/AREngine/Runtime/Runtime.hpp
        include/AREngine/Runtime/RuntimeConfig.hpp
        include/AREngine/Runtime/DesktopFrameDriver.hpp
)

target_include_directories(arengine_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(arengine_runtime
    PUBLIC
        AREngine::Core
        AREngine::Frame
        AREngine::Platform
        AREngine::Rendering
        AREngine::Scene
        AREngine::Assets
        AREngine::Input
        AREngine::XR
)

target_compile_features(arengine_runtime PUBLIC cxx_std_20)

# Runtime is the only place allowed to depend on every engine module at
# once. It owns the main loop and a FrameDriver (Desktop today, XR
# later) — see docs/ARCHITECTURE.md. Editor and Sandbox depend on
# Runtime; nothing in engine/ or runtime/ may depend on editor/ or
# sandbox/.
