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
//   - WindowDesc.hpp         window configuration
//   - Window.hpp              the generic Window interface + CreateAppWindow
//   - WindowCloseEvent.hpp
//   - WindowResizeEvent.hpp
//   - Clock.hpp               SteadyClock
//
// File I/O is deliberately NOT implemented in M2 — nothing in this
// milestone actually needs it (the window demo reads no files). It will
// be added once a real consumer exists (Assets, M6, or earlier if one
// comes up sooner) rather than speculatively now.

#include "AREngine/Platform/WindowDesc.hpp"
#include "AREngine/Platform/Window.hpp"
#include "AREngine/Platform/WindowCloseEvent.hpp"
#include "AREngine/Platform/WindowResizeEvent.hpp"
#include "AREngine/Platform/Clock.hpp"
