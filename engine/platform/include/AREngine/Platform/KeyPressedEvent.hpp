#pragma once

#include "AREngine/Core/Event.hpp"
#include "AREngine/Core/KeyCode.hpp"

namespace AREngine::Platform
{
    // Raised on every WM_KEYDOWN, including Windows' own OS key-repeat
    // messages generated while a key is held — this event does NOT by
    // itself mean "the key just transitioned from up to down." Filtering
    // repeats into a true up->down transition is Input::InputSystem's
    // job (it already has to track per-key held state to do that), not
    // Platform's — see docs/ARCHITECTURE.md, "Key Repeat".
    class KeyPressedEvent : public Core::Event
    {
    public:
        explicit KeyPressedEvent(Core::KeyCode key) : key(key) {}

        Core::KeyCode key;
    };
}
