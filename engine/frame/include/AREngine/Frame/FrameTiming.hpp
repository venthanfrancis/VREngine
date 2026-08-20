#pragma once

namespace AREngine::Frame
{
    // Timing information for a single frame, provided by whichever
    // FrameDriver is currently in use (desktop today, XR later).
    struct FrameTiming
    {
        // Time elapsed since the previous frame, in seconds.
        float deltaTimeSeconds = 0.0f;

        // Time elapsed since the application started, in seconds.
        float totalTimeSeconds = 0.0f;

        // Placeholder for a future XR-style predicted display time: the
        // moment this frame's image is expected to actually be shown to
        // the user, which can be later than "now" once frame prediction
        // is real. Unused (0) on desktop. Deliberately a plain double,
        // not an OpenXR type — the XR module will translate OpenXR's
        // time representation into this later.
        double predictedDisplayTimeSeconds = 0.0;
    };
}
