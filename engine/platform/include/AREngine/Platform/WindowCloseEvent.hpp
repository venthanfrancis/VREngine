#pragma once

#include "AREngine/Core/Event.hpp"

namespace AREngine::Platform
{
    // Raised when something (usually the user clicking the close
    // button) has asked a Window to close. Carries no data —
    // Window::ShouldClose() is the actual source of truth; this is just
    // the notification, delivered at the moment the request happens.
    //
    // Lives in Platform, not Core: this is a window-specific concept,
    // and Core must stay generic (see docs/ARCHITECTURE.md). Platform
    // already depends on Core, so a concrete event type deriving from
    // Core::Event belongs here.
    class WindowCloseEvent : public Core::Event
    {
    };
}
