// Automated tests for ARDemo::MaterialRegistry (MaterialRegistry.hpp) —
// M13. Gated under ARENGINE_ENABLE_VULKAN only because VkDescriptorSet
// is a Vulkan type - no real Vulkan API call is made anywhere in this
// file (mirrors arengine_vulkan_tests' own "pure-logic, no real
// device/GPU required at runtime" pattern). VkDescriptorSet is just an
// opaque handle, so Register/Resolve are tested with small fake
// non-null values rather than a real device.

#include "MaterialRegistry.hpp"

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

    using namespace AREngine::Scene;
    using namespace ARDemo;

    // Small fake non-null descriptor-set "handles" - never passed to a
    // real Vulkan call, only stored/retrieved by MaterialRegistry.
    VkDescriptorSet FakeDescriptorSet(std::uintptr_t value)
    {
        return reinterpret_cast<VkDescriptorSet>(value);
    }

    void TestRegisterThenResolveSucceeds()
    {
        MaterialRegistry registry;
        const VkDescriptorSet fake = FakeDescriptorSet(1);
        registry.Register(MaterialId{1}, fake);

        Check(registry.Resolve(MaterialId{1}) == fake, "Resolve returns exactly what was Registered for that MaterialId");
    }

    void TestUnknownMaterialIdResolvesToNull()
    {
        MaterialRegistry registry;
        Check(registry.Resolve(MaterialId{999}) == VK_NULL_HANDLE, "An unregistered MaterialId resolves to VK_NULL_HANDLE, not a crash");
    }

    void TestZeroMaterialIdResolvesToNullWhenUnregistered()
    {
        MaterialRegistry registry;
        Check(registry.Resolve(MaterialId{}) == VK_NULL_HANDLE,
              "The default/invalid MaterialId{0} behaves like any other unregistered id - no special-cased crash or value");
    }

    void TestTwoMaterialIdsResolveDistinctly()
    {
        MaterialRegistry registry;
        const VkDescriptorSet fakeA = FakeDescriptorSet(1);
        const VkDescriptorSet fakeB = FakeDescriptorSet(2);
        registry.Register(MaterialId{1}, fakeA);
        registry.Register(MaterialId{2}, fakeB);

        Check(registry.Resolve(MaterialId{1}) == fakeA, "MaterialId{1} resolves to its own registered value");
        Check(registry.Resolve(MaterialId{2}) == fakeB, "MaterialId{2} resolves to its own, distinct registered value");
    }

    void TestMultipleEntitiesCanReferenceSameMaterialId()
    {
        // The registry itself has no notion of "entities" - this test
        // proves the same MaterialId can be Resolved repeatedly,
        // consistently, which is what lets many Scene entities share
        // one material resource.
        MaterialRegistry registry;
        const VkDescriptorSet shared = FakeDescriptorSet(7);
        registry.Register(MaterialId{5}, shared);

        Check(registry.Resolve(MaterialId{5}) == shared, "First lookup resolves to the shared descriptor set");
        Check(registry.Resolve(MaterialId{5}) == shared, "Repeated lookups for the same MaterialId are stable and consistent");
    }

    void TestReRegistrationOverwrites()
    {
        // Last-write-wins, matching MeshRegistry::Register's own
        // precedent exactly - not an assert, not a rejected duplicate.
        MaterialRegistry registry;
        const VkDescriptorSet first = FakeDescriptorSet(1);
        const VkDescriptorSet second = FakeDescriptorSet(2);
        registry.Register(MaterialId{1}, first);
        registry.Register(MaterialId{1}, second);

        Check(registry.Resolve(MaterialId{1}) == second, "Re-registering the same MaterialId overwrites the previous value (last-write-wins)");
    }
}

int main()
{
    TestRegisterThenResolveSucceeds();
    TestUnknownMaterialIdResolvesToNull();
    TestZeroMaterialIdResolvesToNullWhenUnregistered();
    TestTwoMaterialIdsResolveDistinctly();
    TestMultipleEntitiesCanReferenceSameMaterialId();
    TestReRegistrationOverwrites();

    if (g_failureCount == 0)
    {
        std::printf("All MaterialRegistry checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
