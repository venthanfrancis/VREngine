#pragma once

#include "AREngine/Core/Event.hpp"
#include "AREngine/Core/KeyCode.hpp"

namespace AREngine::Platform
{
    // Raised on WM_KEYUP. Unlike WM_KEYDOWN, Windows does not repeat
    // this while a key is held, so one KeyReleasedEvent always means
    // one physical release.
    class KeyReleasedEvent : public Core::Event
    {
    public:
        explicit KeyReleasedEvent(Core::KeyCode key) : key(key) {}

        Core::KeyCode key;
    };
}
