#pragma once

#include "AREngine/Core/Event.hpp"
#include "AREngine/Core/KeyCode.hpp"
#include "AREngine/Core/Math/Vec2.hpp"
#include "AREngine/Core/MouseButton.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace AREngine::Input
{
    // Separates physical input devices from engine/game actions: raw
    // keyboard/mouse state is queryable directly (IsKeyDown, ...), and
    // a small action-mapping layer lets gameplay ask "is Select down?"
    // instead of "is Space or Left Mouse down?" — see
    // docs/ARCHITECTURE.md, "M7 Implementation Notes".
    //
    // Depends on Core and Platform (for KeyCode/MouseButton and the
    // generic input event types — see docs/ARCHITECTURE.md, "KeyCode /
    // MouseButton Ownership" and "Input -> Platform Dependency"). Knows
    // nothing about Win32, Scene, Rendering, gameplay entities, or XR —
    // it only ever receives events fed to it through OnEvent(), never
    // reaches into a Window itself.
    //
    // Not a singleton: an application owns an InputSystem instance
    // explicitly (Runtime does — see docs/ARCHITECTURE.md, "Event
    // Routing").
    class InputSystem
    {
    public:
        // Clears the PREVIOUS frame's transient Pressed/Released flags
        // and snapshots the current mouse position as this frame's
        // delta baseline. Must be called before this frame's events are
        // processed (i.e. before Window::PollEvents()) — see
        // docs/ARCHITECTURE.md, "Frame Lifecycle for Transient State".
        void BeginFrame();

        // Feeds one generic engine event into InputSystem. Recognizes
        // KeyPressedEvent/KeyReleasedEvent/MouseButtonPressedEvent/
        // MouseButtonReleasedEvent/MouseMovedEvent/WindowFocusLostEvent
        // (all Platform types) and silently ignores anything else (e.g.
        // WindowCloseEvent/WindowResizeEvent) — see
        // docs/ARCHITECTURE.md, "Window Resize/Close Are Unaffected".
        void OnEvent(Core::Event& event);

        // "Held" — true for every frame the key/button stays down,
        // including the frame it was pressed and the frame before it's
        // released.
        [[nodiscard]] bool IsKeyDown(Core::KeyCode key) const;
        [[nodiscard]] bool IsMouseButtonDown(Core::MouseButton button) const;

        // "Transitioned this frame" — true for exactly one frame: the
        // one in which the up->down (WasPressed) or down->up
        // (WasReleased) transition was observed. See
        // docs/ARCHITECTURE.md, "Held vs. Pressed vs. Released".
        [[nodiscard]] bool WasKeyPressed(Core::KeyCode key) const;
        [[nodiscard]] bool WasKeyReleased(Core::KeyCode key) const;
        [[nodiscard]] bool WasMouseButtonPressed(Core::MouseButton button) const;
        [[nodiscard]] bool WasMouseButtonReleased(Core::MouseButton button) const;

        // Client-area coordinates: origin top-left, +x right, +y down —
        // see Platform::MouseMovedEvent. GetMouseDelta() is the net
        // movement since the start of this frame (not since the last
        // individual mouse-move message) — see docs/ARCHITECTURE.md,
        // "Mouse First-Movement Rule".
        [[nodiscard]] Core::Math::Vec2 GetMousePosition() const;
        [[nodiscard]] Core::Math::Vec2 GetMouseDelta() const;

        // --- Actions ---
        //
        // Binds `key`/`button` to the named action, creating the action
        // if it doesn't exist yet. An action may have any number of
        // bindings (keyboard and/or mouse); IsActionDown/WasAction*
        // are true if ANY bound input satisfies the query — see
        // docs/ARCHITECTURE.md, "Action Mapping / Multiple-Binding
        // Semantics".
        void BindActionKey(const std::string& actionName, Core::KeyCode key);
        void BindActionMouseButton(const std::string& actionName, Core::MouseButton button);

        // An action name with no bindings (including one that was never
        // registered at all) behaves as "always false" — never an
        // error. See docs/ARCHITECTURE.md.
        [[nodiscard]] bool IsActionDown(const std::string& actionName) const;
        [[nodiscard]] bool WasActionPressed(const std::string& actionName) const;
        [[nodiscard]] bool WasActionReleased(const std::string& actionName) const;

    private:
        struct ButtonState
        {
            bool isDown = false;
            bool wasPressed = false;
            bool wasReleased = false;
        };

        struct ActionBindings
        {
            std::vector<Core::KeyCode> keys;
            std::vector<Core::MouseButton> mouseButtons;
        };

        void SetKeyDown(Core::KeyCode key, bool down);
        void SetMouseButtonDown(Core::MouseButton button, bool down);
        void HandleMouseMoved(Core::Math::Vec2 newPosition);
        void HandleFocusLost();

        std::unordered_map<Core::KeyCode, ButtonState> m_keys;
        std::unordered_map<Core::MouseButton, ButtonState> m_mouseButtons;
        std::unordered_map<std::string, ActionBindings> m_actions;

        Core::Math::Vec2 m_mousePosition;
        Core::Math::Vec2 m_frameStartMousePosition;
        bool m_hasMousePosition = false;
    };
}
