#include "AREngine/Input/Input.hpp"

#include "AREngine/Platform/KeyPressedEvent.hpp"
#include "AREngine/Platform/KeyReleasedEvent.hpp"
#include "AREngine/Platform/MouseButtonPressedEvent.hpp"
#include "AREngine/Platform/MouseButtonReleasedEvent.hpp"
#include "AREngine/Platform/MouseMovedEvent.hpp"
#include "AREngine/Platform/WindowFocusLostEvent.hpp"

namespace AREngine::Input
{
    void InputSystem::BeginFrame()
    {
        for (auto& [key, state] : m_keys)
        {
            state.wasPressed = false;
            state.wasReleased = false;
        }
        for (auto& [button, state] : m_mouseButtons)
        {
            state.wasPressed = false;
            state.wasReleased = false;
        }

        // Snapshot "where the mouse was as of the start of this frame,"
        // so GetMouseDelta() reports net movement across the whole
        // frame rather than just the most recent WM_MOUSEMOVE message —
        // several of those can arrive per PollEvents() call. Harmless
        // to do even if the mouse has never moved yet (m_mousePosition
        // is still its default); see HandleMouseMoved for how the very
        // first movement establishes a clean baseline regardless.
        m_frameStartMousePosition = m_mousePosition;
    }

    void InputSystem::OnEvent(Core::Event& event)
    {
        if (auto* keyPressed = dynamic_cast<Platform::KeyPressedEvent*>(&event))
        {
            SetKeyDown(keyPressed->key, true);
        }
        else if (auto* keyReleased = dynamic_cast<Platform::KeyReleasedEvent*>(&event))
        {
            SetKeyDown(keyReleased->key, false);
        }
        else if (auto* mousePressed = dynamic_cast<Platform::MouseButtonPressedEvent*>(&event))
        {
            SetMouseButtonDown(mousePressed->button, true);
        }
        else if (auto* mouseReleased = dynamic_cast<Platform::MouseButtonReleasedEvent*>(&event))
        {
            SetMouseButtonDown(mouseReleased->button, false);
        }
        else if (auto* mouseMoved = dynamic_cast<Platform::MouseMovedEvent*>(&event))
        {
            HandleMouseMoved(mouseMoved->position);
        }
        else if (dynamic_cast<Platform::WindowFocusLostEvent*>(&event) != nullptr)
        {
            HandleFocusLost();
        }
        // Anything else (WindowCloseEvent, WindowResizeEvent, ...) is
        // not an input event and is silently ignored — InputSystem
        // never consumes or interferes with it.
    }

    void InputSystem::SetKeyDown(Core::KeyCode key, bool down)
    {
        ButtonState& state = m_keys[key];

        if (down)
        {
            if (!state.isDown)
            {
                state.isDown = true;
                state.wasPressed = true;
            }
            // else: OS key-repeat while already held — a no-op, per the
            // documented "Pressed means an up->down transition only"
            // rule. The key simply remains Down.
        }
        else if (state.isDown)
        {
            state.isDown = false;
            state.wasReleased = true;
        }
    }

    void InputSystem::SetMouseButtonDown(Core::MouseButton button, bool down)
    {
        ButtonState& state = m_mouseButtons[button];

        if (down)
        {
            if (!state.isDown)
            {
                state.isDown = true;
                state.wasPressed = true;
            }
        }
        else if (state.isDown)
        {
            state.isDown = false;
            state.wasReleased = true;
        }
    }

    void InputSystem::HandleMouseMoved(Core::Math::Vec2 newPosition)
    {
        if (!m_hasMousePosition)
        {
            // The first-ever observed position establishes the
            // baseline and reports zero delta for the frame it arrives
            // in, rather than a large, meaningless jump from (0,0).
            m_hasMousePosition = true;
            m_mousePosition = newPosition;
            m_frameStartMousePosition = newPosition;
            return;
        }

        m_mousePosition = newPosition;
    }

    void InputSystem::HandleFocusLost()
    {
        // The matching key-up/button-up for anything held when focus is
        // lost may never arrive at this window, so every currently-held
        // key/button is synthesized as released here rather than left
        // stuck Down forever. WasReleased is set (not just isDown
        // cleared) so code watching for release transitions still sees
        // one, instead of the key silently going Down -> (nothing).
        for (auto& [key, state] : m_keys)
        {
            if (state.isDown)
            {
                state.isDown = false;
                state.wasReleased = true;
            }
        }
        for (auto& [button, state] : m_mouseButtons)
        {
            if (state.isDown)
            {
                state.isDown = false;
                state.wasReleased = true;
            }
        }
    }

    bool InputSystem::IsKeyDown(Core::KeyCode key) const
    {
        const auto it = m_keys.find(key);
        return it != m_keys.end() && it->second.isDown;
    }

    bool InputSystem::WasKeyPressed(Core::KeyCode key) const
    {
        const auto it = m_keys.find(key);
        return it != m_keys.end() && it->second.wasPressed;
    }

    bool InputSystem::WasKeyReleased(Core::KeyCode key) const
    {
        const auto it = m_keys.find(key);
        return it != m_keys.end() && it->second.wasReleased;
    }

    bool InputSystem::IsMouseButtonDown(Core::MouseButton button) const
    {
        const auto it = m_mouseButtons.find(button);
        return it != m_mouseButtons.end() && it->second.isDown;
    }

    bool InputSystem::WasMouseButtonPressed(Core::MouseButton button) const
    {
        const auto it = m_mouseButtons.find(button);
        return it != m_mouseButtons.end() && it->second.wasPressed;
    }

    bool InputSystem::WasMouseButtonReleased(Core::MouseButton button) const
    {
        const auto it = m_mouseButtons.find(button);
        return it != m_mouseButtons.end() && it->second.wasReleased;
    }

    Core::Math::Vec2 InputSystem::GetMousePosition() const
    {
        return m_mousePosition;
    }

    Core::Math::Vec2 InputSystem::GetMouseDelta() const
    {
        return m_mousePosition - m_frameStartMousePosition;
    }

    void InputSystem::BindActionKey(const std::string& actionName, Core::KeyCode key)
    {
        m_actions[actionName].keys.push_back(key);
    }

    void InputSystem::BindActionMouseButton(const std::string& actionName, Core::MouseButton button)
    {
        m_actions[actionName].mouseButtons.push_back(button);
    }

    bool InputSystem::IsActionDown(const std::string& actionName) const
    {
        const auto it = m_actions.find(actionName);
        if (it == m_actions.end())
        {
            return false;
        }

        for (const Core::KeyCode key : it->second.keys)
        {
            if (IsKeyDown(key))
            {
                return true;
            }
        }
        for (const Core::MouseButton button : it->second.mouseButtons)
        {
            if (IsMouseButtonDown(button))
            {
                return true;
            }
        }
        return false;
    }

    bool InputSystem::WasActionPressed(const std::string& actionName) const
    {
        const auto it = m_actions.find(actionName);
        if (it == m_actions.end())
        {
            return false;
        }

        for (const Core::KeyCode key : it->second.keys)
        {
            if (WasKeyPressed(key))
            {
                return true;
            }
        }
        for (const Core::MouseButton button : it->second.mouseButtons)
        {
            if (WasMouseButtonPressed(button))
            {
                return true;
            }
        }
        return false;
    }

    bool InputSystem::WasActionReleased(const std::string& actionName) const
    {
        const auto it = m_actions.find(actionName);
        if (it == m_actions.end())
        {
            return false;
        }

        for (const Core::KeyCode key : it->second.keys)
        {
            if (WasKeyReleased(key))
            {
                return true;
            }
        }
        for (const Core::MouseButton button : it->second.mouseButtons)
        {
            if (WasMouseButtonReleased(button))
            {
                return true;
            }
        }
        return false;
    }
}
