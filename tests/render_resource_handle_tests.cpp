// M16 pure-logic tests for MeshHandleAllocator/MaterialHandleAllocator
// (engine/rendering/src/HandleAllocators.hpp/.cpp) - the id-minting/
// caching bookkeeping VulkanRenderResourceContext uses internally,
// extracted specifically so it can be unit-tested without a GPU (real
// mesh/texture upload can't be - the same limitation MeshCache/
// TextureCache always had). No human interaction, fully headless, no
// Vulkan dependency. See docs/ARCHITECTURE.md, "M16 - Render Resource
// Context Foundation".

#include "HandleAllocators.hpp"

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

    using namespace AREngine::Rendering;
    using AREngine::Assets::AssetId;

    void TestMeshHandleAllocatorGetOrCreateReturnsValidHandle()
    {
        MeshHandleAllocator allocator;
        const MeshHandle handle = allocator.GetOrCreate(AssetId{1});
        Check(handle.IsValid(), "GetOrCreate returns a valid (nonzero) handle for a valid AssetId");
    }

    void TestMeshHandleAllocatorIsIdempotentPerAssetId()
    {
        MeshHandleAllocator allocator;
        const MeshHandle first = allocator.GetOrCreate(AssetId{1});
        const MeshHandle second = allocator.GetOrCreate(AssetId{1});
        Check(first == second, "Calling GetOrCreate twice with the same AssetId returns the identical MeshHandle");
    }

    void TestMeshHandleAllocatorDistinctAssetIdsGetDistinctHandles()
    {
        MeshHandleAllocator allocator;
        const MeshHandle first = allocator.GetOrCreate(AssetId{1});
        const MeshHandle second = allocator.GetOrCreate(AssetId{2});
        Check(!(first == second), "Two different AssetIds receive two distinct MeshHandles");
    }

    void TestMeshHandleAllocatorCountReflectsDistinctAssetIdsNotCallCount()
    {
        MeshHandleAllocator allocator;
        // 5 calls, referencing only 2 distinct AssetIds - mirrors "2
        // mesh AssetIds referenced by 5 entities -> 2 mesh resources".
        (void)allocator.GetOrCreate(AssetId{1});
        (void)allocator.GetOrCreate(AssetId{2});
        (void)allocator.GetOrCreate(AssetId{1});
        (void)allocator.GetOrCreate(AssetId{1});
        (void)allocator.GetOrCreate(AssetId{2});
        Check(allocator.Count() == 2, "Count() reflects the number of distinct AssetIds seen, not the number of calls");
    }

    void TestMeshHandleAllocatorCreateNewAlwaysMintsFreshAndSharesCounter()
    {
        MeshHandleAllocator allocator;
        const MeshHandle assetBacked = allocator.GetOrCreate(AssetId{1});
        const MeshHandle proceduralFirst = allocator.CreateNew();
        const MeshHandle proceduralSecond = allocator.CreateNew();
        Check(!(assetBacked == proceduralFirst) && !(proceduralFirst == proceduralSecond) && !(assetBacked == proceduralSecond),
              "CreateNew() mints a fresh handle every call, never colliding with an asset-backed handle from the same allocator");
        Check(allocator.Count() == 3, "Count() includes both asset-backed and procedural handles");
    }

    void TestMaterialHandleAllocatorCreateNewReturnsValidHandle()
    {
        MaterialHandleAllocator allocator;
        const MaterialHandle handle = allocator.CreateNew();
        Check(handle.IsValid(), "CreateNew returns a valid (nonzero) handle");
    }

    void TestMaterialHandleAllocatorIsNeverIdempotent()
    {
        // Unlike MeshHandleAllocator, MaterialHandleAllocator has no
        // AssetId to key by at all - every call mints a fresh handle,
        // which is exactly the property that lets "the same texture
        // asset" back multiple independent MaterialHandles.
        MaterialHandleAllocator allocator;
        const MaterialHandle first = allocator.CreateNew();
        const MaterialHandle second = allocator.CreateNew();
        Check(!(first == second), "Two CreateNew() calls always mint two distinct MaterialHandles");
    }

    void TestMaterialHandleAllocatorCountReflectsCallCount()
    {
        MaterialHandleAllocator allocator;
        (void)allocator.CreateNew();
        (void)allocator.CreateNew();
        (void)allocator.CreateNew();
        Check(allocator.Count() == 3, "Count() reflects the number of CreateNew() calls made");
    }

    void TestDefaultConstructedHandlesAreInvalid()
    {
        Check(!MeshHandle{}.IsValid(), "A default-constructed MeshHandle is never valid");
        Check(!MaterialHandle{}.IsValid(), "A default-constructed MaterialHandle is never valid");
    }

    void TestMintedHandlesAreNeverTheInvalidValue()
    {
        MeshHandleAllocator meshAllocator;
        Check(meshAllocator.GetOrCreate(AssetId{1}).IsValid(), "A freshly-minted MeshHandle from GetOrCreate is never the invalid (0) value");
        Check(meshAllocator.CreateNew().IsValid(), "A freshly-minted MeshHandle from CreateNew is never the invalid (0) value");

        MaterialHandleAllocator materialAllocator;
        Check(materialAllocator.CreateNew().IsValid(), "A freshly-minted MaterialHandle is never the invalid (0) value");
    }
}

int main()
{
    TestMeshHandleAllocatorGetOrCreateReturnsValidHandle();
    TestMeshHandleAllocatorIsIdempotentPerAssetId();
    TestMeshHandleAllocatorDistinctAssetIdsGetDistinctHandles();
    TestMeshHandleAllocatorCountReflectsDistinctAssetIdsNotCallCount();
    TestMeshHandleAllocatorCreateNewAlwaysMintsFreshAndSharesCounter();
    TestMaterialHandleAllocatorCreateNewReturnsValidHandle();
    TestMaterialHandleAllocatorIsNeverIdempotent();
    TestMaterialHandleAllocatorCountReflectsCallCount();
    TestDefaultConstructedHandlesAreInvalid();
    TestMintedHandlesAreNeverTheInvalidValue();

    if (g_failureCount == 0)
    {
        std::printf("All M16 render-resource-handle checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
