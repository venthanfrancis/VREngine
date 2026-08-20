#pragma once

// AREngine::Core events.
//
// Just enough infrastructure to represent an engine event cleanly.
// Concrete event types (e.g. a future WindowResized or KeyPressed) will
// derive from Event once Platform/Input have real requirements to
// model — see docs/ROADMAP.md.
//
// Deliberately does NOT include a dispatcher, a listener list, a
// subscription/ownership model, or any reflection/template machinery.
// Those are added later, once a real use case defines what they need to
// look like — not speculatively now.

namespace AREngine::Core
{
    class Event
    {
    public:
        virtual ~Event() = default;

        // Set by a handler that has fully processed this event, so a
        // future dispatch mechanism can skip already-handled events.
        bool handled = false;
    };
}
