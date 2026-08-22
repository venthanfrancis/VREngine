#pragma once

// AREngine::Platform — convenience umbrella header.
//
// Depends only on Core. The only module allowed to call OS-specific
// APIs — see docs/ARCHITECTURE.md. Public headers here never expose
// Win32 (or other OS) types; Win32 implementation details live entirely
// under src/windows/ and are never included by anything outside this
// module.
//
// Includes:
//   - WindowDesc.hpp                  window configuration
//   - NativeWindowHandle.hpp           low-level escape hatch for Vulkan/OpenXR (M8B)
//   - Window.hpp                       the generic Window interface + CreateAppWindow
//   - WindowCloseEvent.hpp
//   - WindowResizeEvent.hpp
//   - WindowFocusLostEvent.hpp         (M7)
//   - KeyPressedEvent.hpp              (M7)
//   - KeyReleasedEvent.hpp             (M7)
//   - MouseButtonPressedEvent.hpp      (M7)
//   - MouseButtonReleasedEvent.hpp     (M7)
//   - MouseMovedEvent.hpp              (M7)
//   - Clock.hpp                        SteadyClock
//
// File I/O is deliberately NOT implemented — nothing so far needs it
// (see docs/ARCHITECTURE.md, Section 10). It will be added once a real
// consumer exists rather than speculatively now.

#include "AREngine/Platform/WindowDesc.hpp"
#include "AREngine/Platform/NativeWindowHandle.hpp"
#include "AREngine/Platform/Window.hpp"
#include "AREngine/Platform/WindowCloseEvent.hpp"
#include "AREngine/Platform/WindowResizeEvent.hpp"
#include "AREngine/Platform/WindowFocusLostEvent.hpp"
#include "AREngine/Platform/KeyPressedEvent.hpp"
#include "AREngine/Platform/KeyReleasedEvent.hpp"
#include "AREngine/Platform/MouseButtonPressedEvent.hpp"
#include "AREngine/Platform/MouseButtonReleasedEvent.hpp"
#include "AREngine/Platform/MouseMovedEvent.hpp"
#include "AREngine/Platform/Clock.hpp"
