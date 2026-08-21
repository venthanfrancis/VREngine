#pragma once

namespace AREngine::Core
{
    // Engine-owned keyboard key identity. Deliberately not every key a
    // keyboard has — just the ones genuinely needed so far. Lives in
    // Core (not Input or Platform) because both Platform (which
    // translates Win32 virtual-key codes into these) and Input (which
    // uses these to represent key state) need it, and neither should
    // depend on the other for it — see docs/ARCHITECTURE.md, "KeyCode /
    // MouseButton Ownership".
    enum class KeyCode
    {
        Unknown = 0,

        A, B, C, D, E, F, G, H, I, J, K, L, M,
        N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

        Escape,
        Space,
        Enter,
        Tab,

        Up,
        Down,
        Left,
        Right,

        LeftShift,
        RightShift,
        LeftCtrl,
        RightCtrl,
    };
}
