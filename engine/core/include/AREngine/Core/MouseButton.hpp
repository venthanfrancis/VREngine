#pragma once

namespace AREngine::Core
{
    // Engine-owned mouse button identity — see KeyCode.hpp for why this
    // lives in Core.
    enum class MouseButton
    {
        Unknown = 0,
        Left,
        Right,
        Middle,
    };
}
