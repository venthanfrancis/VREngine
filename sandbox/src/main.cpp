// AREngine Sandbox — the first real consumer of Runtime (M3).
//
// Constructs a Runtime with a simple config and runs it. Sandbox
// includes no Win32 headers, constructs no WindowsWindow, and knows
// nothing about Win32 — it only ever sees Runtime's public API.

#include "AREngine/Runtime/Runtime.hpp"
#include "AREngine/Runtime/RuntimeConfig.hpp"

int main()
{
    AREngine::Runtime::RuntimeConfig config;
    config.windowTitle = "AREngine Sandbox";
    config.windowWidth = 1280;
    config.windowHeight = 720;

    AREngine::Runtime::Runtime runtime(config);
    runtime.Run();

    return 0;
}
