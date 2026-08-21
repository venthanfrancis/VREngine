#pragma once

#include "AREngine/Core/Event.hpp"
#include "AREngine/Core/Math/Vec2.hpp"

namespace AREngine::Platform
{
    // Raised on WM_MOUSEMOVE. Position is in client-area coordinates:
    // origin at the top-left, +x right, +y down — a 2D desktop-window
    // convention, deliberately separate from (and not to be confused
    // with) the engine's 3D world convention (+Y up) documented in
    // docs/WORLD_CONVENTIONS.md. See docs/ARCHITECTURE.md, "Mouse
    // Coordinate Convention".
    class MouseMovedEvent : public Core::Event
    {
    public:
        explicit MouseMovedEvent(Core::Math::Vec2 position) : position(position) {}

        Core::Math::Vec2 position;
    };
}
