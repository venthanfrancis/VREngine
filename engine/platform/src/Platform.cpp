#include "AREngine/Platform/Platform.hpp"

// Platform.hpp aggregates the generic (non-Win32) public API. This file
// exists so it gets a standalone compile check. Window's actual
// implementation lives under src/windows/ and is never included here —
// this translation unit stays OS-agnostic on purpose.
