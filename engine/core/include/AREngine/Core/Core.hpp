#pragma once

// AREngine::Core — convenience umbrella header.
//
// The foundational module. Depends on nothing else in the engine (see
// docs/ARCHITECTURE.md). Includes:
//   - Log.hpp          logging (AR_LOG_* macros)
//   - Assert.hpp        assertions (AR_ASSERT / AR_ASSERT_MSG)
//   - Event.hpp         the Event base type
//   - KeyCode.hpp        keyboard key vocabulary (M7)
//   - MouseButton.hpp    mouse button vocabulary (M7)
//   - Math/Math.hpp      Vec2, Vec3, Vec4, Quaternion, Mat4
//
// Use std:: containers (vector, unordered_map, string, ...) directly
// throughout the engine. Do not introduce custom container types without
// a measured (profiled) reason.

#include "AREngine/Core/Log.hpp"
#include "AREngine/Core/Assert.hpp"
#include "AREngine/Core/Event.hpp"
#include "AREngine/Core/KeyCode.hpp"
#include "AREngine/Core/MouseButton.hpp"
#include "AREngine/Core/Math/Math.hpp"
