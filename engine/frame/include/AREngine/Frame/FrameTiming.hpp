#pragma once

namespace AREngine::Frame
{
    // Timing information for a single frame, provided by whichever
    // FrameDriver is currently in use (desktop today, XR later).
    //
    // All three fields are double, not float: totalTimeSeconds
    // accumulates for the entire lifetime of a running engine session,
    // and float's ~7 significant digits start losing meaningful
    // precision after a few hours of continuous runtime. Double
    // precision is cheap and removes that limit. This is specific to
    // timing — Vec3/Mat4/Quaternion and general rendering/transform math
    // stay float.
    struct FrameTiming
    {
        // Time elapsed since the previous frame, in seconds.
        double deltaTimeSeconds = 0.0;

        // Time elapsed since the application started, in seconds.
        double totalTimeSeconds = 0.0;

        // Placeholder for a future XR-style predicted display time: the
        // moment this frame's image is expected to actually be shown to
        // the user, which can be later than "now" once frame prediction
        // is real. Unused (0) on desktop. Deliberately a plain double,
        // not an OpenXR type — the XR module will translate OpenXR's
        // time representation into this later.
        double predictedDisplayTimeSeconds = 0.0;
    };
}
