#include "AREngine/Frame/Frame.hpp"

// Frame's types (FrameTiming, ViewInfo, FrameDriver) are currently pure
// data/interface headers with no implementation of their own. This
// translation unit exists so the module still builds as a normal static
// library, and so the umbrella header gets a standalone compile check.
