#pragma once

// M16: id-minting/caching bookkeeping for VulkanRenderResourceContext,
// extracted into its own deliberately Vulkan-free type so it can be
// unit-tested without a GPU - real mesh/texture upload can't be
// (the same limitation MeshCache/TextureCache always had), but the
// AssetId -> handle bookkeeping itself has no such constraint. Kept
// unconditional (not gated behind ARENGINE_ENABLE_VULKAN) so its tests
// run in every build configuration. See docs/ARCHITECTURE.md, "M16 -
// Render Resource Context Foundation".
//
// Deliberately two small, separately-named, ungeneralized classes
// rather than one templated allocator: the two have genuinely
// different semantics (idempotent-per-AssetId vs. always-fresh), and
// there are only ever two call sites - a template would be
// abstraction for its own sake.

#include "AREngine/Assets/AssetId.hpp"
#include "AREngine/Rendering/Handles.hpp"

#include <cstdint>
#include <unordered_map>

namespace AREngine::Rendering
{
    // Mints a MeshHandle per distinct AssetId, idempotently - calling
    // GetOrCreate twice with the same AssetId returns the SAME handle,
    // matching AssetId's own per-instance-caching philosophy (see
    // AssetManager). CreateNew() mints a handle for content with no
    // AssetId to key by (procedural geometry) - it shares the same
    // monotonic counter as GetOrCreate, so asset-backed and procedural
    // mesh handles can never collide.
    class MeshHandleAllocator
    {
    public:
        [[nodiscard]] MeshHandle GetOrCreate(Assets::AssetId assetId);
        [[nodiscard]] MeshHandle CreateNew();
        [[nodiscard]] std::size_t Count() const { return m_handlesByAsset.size() + m_proceduralCount; }

    private:
        std::unordered_map<Assets::AssetId, MeshHandle> m_handlesByAsset;
        std::size_t m_proceduralCount = 0;
        std::uint64_t m_next = 1;
    };

    // Mints a fresh MaterialHandle on every call - deliberately NOT
    // idempotent per source AssetId, since the same texture asset must
    // remain usable by multiple independent MaterialHandles (e.g. a
    // future per-material state) - see docs/ARCHITECTURE.md, "M16 -
    // Render Resource Context Foundation", "Material Creation".
    class MaterialHandleAllocator
    {
    public:
        [[nodiscard]] MaterialHandle CreateNew();
        [[nodiscard]] std::size_t Count() const { return m_next - 1; }

    private:
        std::uint64_t m_next = 1;
    };
}
