#pragma once

// Private OpenXR bring-up implementation — see OpenXRSession.hpp.
//
// Deliberately no Vulkan dependency, and deliberately no dependency on
// AREngine::Frame either: this file converts OpenXR's own time
// representation (XrTime) into a plain double of seconds, nothing
// more. Frame::FrameTiming.hpp already anticipates this exact
// conversion ("the XR module will translate OpenXR's time
// representation into this later") but M9E does not itself wire XR up
// to a real Frame::FrameTiming/FrameDriver - that depends on the
// FrameDriver-fit evaluation M9E's own brief requires happen AFTER the
// raw OpenXR frame lifecycle is proven out here, not before. See
// docs/ARCHITECTURE.md, "FrameDriver Fit Evaluation (M9E)".

#include <openxr/openxr.h>

namespace AREngine::XR::OpenXR
{
    // The OpenXR spec defines XrTime as a 64-bit signed integer count
    // of nanoseconds - this is NOT runtime- or vendor-specific (unlike,
    // say, its epoch, which the spec deliberately leaves
    // implementation-defined - an XrTime value is only meaningful
    // relative to other XrTime values from the same runtime, never as
    // an absolute wall-clock reading). The nanoseconds-per-tick meaning
    // itself, though, is a fixed part of the spec, so this conversion
    // is exact, not a guess. Pure arithmetic, no API calls - directly
    // unit-testable.
    [[nodiscard]] constexpr double XrTimeToSeconds(XrTime time)
    {
        return static_cast<double>(time) / 1'000'000'000.0;
    }
}
