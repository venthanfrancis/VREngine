#pragma once

// AREngine::Frame — convenience umbrella header.
//
// Depends only on Core. Kept as its own module — separate from Core —
// specifically so Core stays a minimal foundation with no rendering/XR
// concepts, while Runtime, DesktopFrameDriver (later), and XRFrameDriver
// (later) can all depend on Frame without Core needing to know about any
// of them. See docs/ARCHITECTURE.md, "The FrameDriver Abstraction".

#include "AREngine/Frame/FrameTiming.hpp"
#include "AREngine/Frame/FrameStatus.hpp"
#include "AREngine/Frame/FrameContext.hpp"
#include "AREngine/Frame/ViewInfo.hpp"
#include "AREngine/Frame/FrameDriver.hpp"
