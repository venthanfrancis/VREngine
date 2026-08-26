add_library(arengine_input STATIC
    src/Input.cpp
)
add_library(AREngine::Input ALIAS arengine_input)

target_sources(arengine_input
    PRIVATE
        include/AREngine/Input/Input.hpp
        # M10: generic action-state vocabulary (digital/analog/vector2/
        # pose) for input sources richer than keyboard/mouse - header-
        # only, no new .cpp needed. See docs/ARCHITECTURE.md, "Generic
        # Input Extension (M10)".
        include/AREngine/Input/ActionState.hpp
)

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

# Input -> Platform is deliberate, not incidental: InputSystem::OnEvent
# recognizes Platform's generic input event types (KeyPressedEvent,
# MouseMovedEvent, ...) via dynamic_cast. This is a downward dependency
# (Input sits above Platform in the layer diagram) and was already the
# established relationship since M0 — see docs/ARCHITECTURE.md, "Why
# Input Depends On Platform". InputSystem never reaches into a Window or
# touches Win32 itself; Runtime is what feeds it events.
