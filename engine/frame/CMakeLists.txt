add_library(arengine_frame STATIC
    src/Frame.cpp
)
add_library(AREngine::Frame ALIAS arengine_frame)

target_sources(arengine_frame
    PRIVATE
        include/AREngine/Frame/Frame.hpp
        include/AREngine/Frame/FrameTiming.hpp
        include/AREngine/Frame/FrameStatus.hpp
        include/AREngine/Frame/FrameContext.hpp
        include/AREngine/Frame/ViewInfo.hpp
        include/AREngine/Frame/FrameDriver.hpp
)

target_include_directories(arengine_frame
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(arengine_frame
    PUBLIC
        AREngine::Core
)

target_compile_features(arengine_frame PUBLIC cxx_std_20)
