// M7 automated tests for AREngine::Input: InputSystem's raw key/mouse
// state, frame lifecycle (BeginFrame clearing transient flags), key
// repeat suppression, focus-loss handling, mouse position/delta, and
// the action-mapping layer. No human interaction and no real Windows
// keyboard — generic engine events are constructed directly and fed to
// OnEvent(), exactly as InputSystem is designed to be tested.

#include "AREngine/Input/Input.hpp"
#include "AREngine/Platform/KeyPressedEvent.hpp"
#include "AREngine/Platform/KeyReleasedEvent.hpp"
#include "AREngine/Platform/MouseButtonPressedEvent.hpp"
#include "AREngine/Platform/MouseButtonReleasedEvent.hpp"
#include "AREngine/Platform/MouseMovedEvent.hpp"
#include "AREngine/Platform/WindowFocusLostEvent.hpp"

#include <cstdio>

namespace
{
    int g_failureCount = 0;

    void Check(bool condition, const char* description)
    {
        if (!condition)
        {
            std::fprintf(stderr, "FAILED: %s\n", description);
            ++g_failureCount;
        }
    }

    using namespace AREngine::Input;
    using AREngine::Core::KeyCode;
    using AREngine::Core::MouseButton;
    using AREngine::Core::Math::Vec2;

    void TestDefaultKeyIsNotDown()
    {
        InputSystem input;
        Check(!input.IsKeyDown(KeyCode::Space), "A key with no events yet is not down");
        Check(!input.WasKeyPressed(KeyCode::Space), "A key with no events yet was not pressed");
        Check(!input.WasKeyReleased(KeyCode::Space), "A key with no events yet was not released");
    }

    void TestPressMakesKeyDownAndPressed()
    {
        InputSystem input;
        input.BeginFrame();

        AREngine::Platform::KeyPressedEvent event(KeyCode::Space);
        input.OnEvent(event);

        Check(input.IsKeyDown(KeyCode::Space), "Pressing Space makes it Down");
        Check(input.WasKeyPressed(KeyCode::Space), "Pressing Space sets WasPressed");
        Check(!input.WasKeyReleased(KeyCode::Space), "Pressing Space does not set WasReleased");
    }

    void TestBeginFrameClearsTransientState()
    {
        InputSystem input;
        input.BeginFrame();
        AREngine::Platform::KeyPressedEvent pressed(KeyCode::Space);
        input.OnEvent(pressed);
        Check(input.WasKeyPressed(KeyCode::Space), "WasPressed is true the frame Space was pressed");

        input.BeginFrame(); // next frame, no new events
        Check(!input.WasKeyPressed(KeyCode::Space), "WasPressed is cleared by the next BeginFrame");
        Check(input.IsKeyDown(KeyCode::Space), "IsKeyDown (held state) survives BeginFrame, unlike WasPressed");
    }

    void TestHeldKeyRemainsDownAcrossFrames()
    {
        InputSystem input;
        input.BeginFrame();
        AREngine::Platform::KeyPressedEvent pressed(KeyCode::A);
        input.OnEvent(pressed);

        for (int i = 0; i < 3; ++i)
        {
            input.BeginFrame();
            Check(input.IsKeyDown(KeyCode::A), "Held key remains Down across multiple frames with no new events");
        }
    }

    void TestRepeatedKeyDownDoesNotRetriggerPressed()
    {
        InputSystem input;
        input.BeginFrame();

        AREngine::Platform::KeyPressedEvent first(KeyCode::Space);
        input.OnEvent(first);
        Check(input.WasKeyPressed(KeyCode::Space), "The first KeyPressedEvent sets WasPressed");

        // Simulate Windows' OS key-repeat: another WM_KEYDOWN-derived
        // event for the same key, still within the same frame, with no
        // release in between.
        AREngine::Platform::KeyPressedEvent repeatSameFrame(KeyCode::Space);
        input.OnEvent(repeatSameFrame);
        Check(input.IsKeyDown(KeyCode::Space), "The key is still Down after a same-frame repeat");

        // Even across a frame boundary, a repeat with no intervening
        // release must not re-trigger Pressed.
        input.BeginFrame();
        AREngine::Platform::KeyPressedEvent repeatNextFrame(KeyCode::Space);
        input.OnEvent(repeatNextFrame);
        Check(!input.WasKeyPressed(KeyCode::Space),
              "A repeat KeyPressedEvent with no prior release does not set WasPressed again");
    }

    void TestReleaseMakesKeyUpAndReleased()
    {
        InputSystem input;
        input.BeginFrame();
        AREngine::Platform::KeyPressedEvent pressed(KeyCode::Space);
        input.OnEvent(pressed);

        input.BeginFrame();
        AREngine::Platform::KeyReleasedEvent released(KeyCode::Space);
        input.OnEvent(released);

        Check(!input.IsKeyDown(KeyCode::Space), "Releasing Space makes it not Down");
        Check(input.WasKeyReleased(KeyCode::Space), "Releasing Space sets WasReleased");
        Check(!input.WasKeyPressed(KeyCode::Space), "Releasing Space does not set WasPressed");
    }

    void TestMouseButtonStates()
    {
        InputSystem input;
        input.BeginFrame();

        Check(!input.IsMouseButtonDown(MouseButton::Left), "Left mouse button starts up");

        AREngine::Platform::MouseButtonPressedEvent pressed(MouseButton::Left);
        input.OnEvent(pressed);
        Check(input.IsMouseButtonDown(MouseButton::Left), "Pressing Left mouse makes it Down");
        Check(input.WasMouseButtonPressed(MouseButton::Left), "Pressing Left mouse sets WasPressed");

        input.BeginFrame();
        AREngine::Platform::MouseButtonReleasedEvent released(MouseButton::Left);
        input.OnEvent(released);
        Check(!input.IsMouseButtonDown(MouseButton::Left), "Releasing Left mouse makes it not Down");
        Check(input.WasMouseButtonReleased(MouseButton::Left), "Releasing Left mouse sets WasReleased");
    }

    void TestMousePositionAndFirstMovementBaseline()
    {
        InputSystem input;
        input.BeginFrame();

        AREngine::Platform::MouseMovedEvent firstMove(Vec2(100.0f, 100.0f));
        input.OnEvent(firstMove);

        Check(input.GetMousePosition() == Vec2(100.0f, 100.0f),
              "GetMousePosition reports the first observed position");
        Check(input.GetMouseDelta() == Vec2(0.0f, 0.0f),
              "The first-ever mouse position establishes the baseline and reports zero delta");
    }

    void TestMouseDelta()
    {
        InputSystem input;
        input.BeginFrame();
        AREngine::Platform::MouseMovedEvent initial(Vec2(100.0f, 100.0f));
        input.OnEvent(initial);

        // A new frame starts with the mouse already at (100,100);
        // BeginFrame snapshots that as this frame's baseline.
        input.BeginFrame();
        AREngine::Platform::MouseMovedEvent moved(Vec2(110.0f, 90.0f));
        input.OnEvent(moved);

        const Vec2 delta = input.GetMouseDelta();
        Check(delta == Vec2(10.0f, -10.0f),
              "Moving from (100,100) to (110,90) gives delta (+10,-10) under +x right, +y down");
    }

    void TestActionWithKeyboardBinding()
    {
        InputSystem input;
        input.BindActionKey("Jump", KeyCode::Space);

        input.BeginFrame();
        Check(!input.IsActionDown("Jump"), "Jump is not down before Space is pressed");

        AREngine::Platform::KeyPressedEvent pressed(KeyCode::Space);
        input.OnEvent(pressed);
        Check(input.IsActionDown("Jump"), "Jump is down while its bound key (Space) is down");
        Check(input.WasActionPressed("Jump"), "WasActionPressed follows the bound key's WasPressed");
    }

    void TestActionWithMouseBinding()
    {
        InputSystem input;
        input.BindActionMouseButton("Fire", MouseButton::Left);

        input.BeginFrame();
        AREngine::Platform::MouseButtonPressedEvent pressed(MouseButton::Left);
        input.OnEvent(pressed);

        Check(input.IsActionDown("Fire"), "Fire is down while its bound mouse button is down");
        Check(input.WasActionPressed("Fire"), "WasActionPressed follows the bound mouse button's WasPressed");
    }

    void TestActionWithMultipleBindings()
    {
        InputSystem keyboardTrigger;
        keyboardTrigger.BindActionKey("Select", KeyCode::Enter);
        keyboardTrigger.BindActionMouseButton("Select", MouseButton::Left);
        keyboardTrigger.BeginFrame();
        AREngine::Platform::KeyPressedEvent enterPressed(KeyCode::Enter);
        keyboardTrigger.OnEvent(enterPressed);
        Check(keyboardTrigger.WasActionPressed("Select"), "Select triggers via its keyboard binding (Enter) alone");

        InputSystem mouseTrigger;
        mouseTrigger.BindActionKey("Select", KeyCode::Enter);
        mouseTrigger.BindActionMouseButton("Select", MouseButton::Left);
        mouseTrigger.BeginFrame();
        AREngine::Platform::MouseButtonPressedEvent leftPressed(MouseButton::Left);
        mouseTrigger.OnEvent(leftPressed);
        Check(mouseTrigger.WasActionPressed("Select"), "Select also triggers via its mouse binding (Left click) alone");
    }

    void TestUnknownActionIsHandledPredictably()
    {
        InputSystem input;
        input.BeginFrame();

        Check(!input.IsActionDown("NeverRegistered"), "An action that was never bound is never down");
        Check(!input.WasActionPressed("NeverRegistered"), "An action that was never bound was never pressed");
        Check(!input.WasActionReleased("NeverRegistered"), "An action that was never bound was never released");
    }

    void TestFocusLossClearsHeldState()
    {
        InputSystem input;
        input.BeginFrame();
        AREngine::Platform::KeyPressedEvent keyPressed(KeyCode::W);
        input.OnEvent(keyPressed);
        AREngine::Platform::MouseButtonPressedEvent mousePressed(MouseButton::Right);
        input.OnEvent(mousePressed);

        Check(input.IsKeyDown(KeyCode::W) && input.IsMouseButtonDown(MouseButton::Right),
              "Sanity check: both are held before focus loss");

        input.BeginFrame();
        AREngine::Platform::WindowFocusLostEvent focusLost;
        input.OnEvent(focusLost);

        Check(!input.IsKeyDown(KeyCode::W), "Focus loss clears a held key");
        Check(!input.IsMouseButtonDown(MouseButton::Right), "Focus loss clears a held mouse button");
        Check(input.WasKeyReleased(KeyCode::W), "Focus loss reports a Released transition for the held key");
        Check(input.WasMouseButtonReleased(MouseButton::Right),
              "Focus loss reports a Released transition for the held mouse button");
    }
}

int main()
{
    TestDefaultKeyIsNotDown();
    TestPressMakesKeyDownAndPressed();
    TestBeginFrameClearsTransientState();
    TestHeldKeyRemainsDownAcrossFrames();
    TestRepeatedKeyDownDoesNotRetriggerPressed();
    TestReleaseMakesKeyUpAndReleased();
    TestMouseButtonStates();
    TestMousePositionAndFirstMovementBaseline();
    TestMouseDelta();
    TestActionWithKeyboardBinding();
    TestActionWithMouseBinding();
    TestActionWithMultipleBindings();
    TestUnknownActionIsHandledPredictably();
    TestFocusLossClearsHeldState();

    if (g_failureCount == 0)
    {
        std::printf("All Input M7 checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
