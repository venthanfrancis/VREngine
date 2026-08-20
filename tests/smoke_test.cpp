#include "AREngine/Core/Core.hpp"

// M0 build-system smoke test — not a real test suite yet.

int main()
{
    return AREngine::Core::ModuleName() != nullptr ? 0 : 1;
}
