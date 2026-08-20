#include "AREngine/Core/Core.hpp"

// Core.hpp aggregates Log, Assert, Event, and Math. Log and Assert have
// their own translation units (Log.cpp, Assert.cpp); this file exists
// mainly so Event.hpp and the header-only Math types get a standalone
// compile check.
