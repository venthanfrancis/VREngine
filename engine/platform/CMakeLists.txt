add_library(arengine_platform STATIC
    src/Platform.cpp
    src/Clock.cpp
)
add_library(AREngine::Platform ALIAS arengine_platform)

target_sources(arengine_platform
    PRIVATE
        include/AREngine/Platform/Platform.hpp
        include/AREngine/Platform/WindowDesc.hpp
        include/AREngine/Platform/Window.hpp
        include/AREngine/Platform/WindowCloseEvent.hpp
        include/AREngine/Platform/WindowResizeEvent.hpp
        include/AREngine/Platform/Clock.hpp
)

target_include_directories(arengine_platform
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(arengine_platform
    PUBLIC
        AREngine::Core
)

target_compile_features(arengine_platform PUBLIC cxx_std_20)

# OS-specific implementation lives entirely inside this module's src/ —
# nothing above Platform should ever call an OS API directly, and no
# Win32 type may appear in include/ (see docs/ARCHITECTURE.md).
if(WIN32)
    target_sources(arengine_platform
        PRIVATE
            src/windows/WindowsWindow.hpp
            src/windows/WindowsWindow.cpp
    )
    target_link_libraries(arengine_platform PRIVATE user32.lib)
else()
    message(FATAL_ERROR
        "AREngine::Platform only has a Windows backend so far. "
        "Linux/Android backends are planned for M14 (see docs/ROADMAP.md) "
        "and would be added here without changing Platform's public headers."
    )
endif()
