#pragma once

#include "AREngine/Rendering/RenderDevice.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace AREngine::Rendering
{
    // A RenderDevice that does no real graphics work: it validates
    // descriptors, hands out handles, tracks what's "live," and counts
    // draws — entirely in std:: containers, no graphics API calls of
    // any kind. Its purpose is to prove the RenderDevice abstraction is
    // usable end to end before a real backend (Vulkan, M8) exists, and
    // to give automated tests something deterministic to check. See
    // docs/ARCHITECTURE.md, "Null Backend".
    class NullRenderDevice final : public RenderDevice
    {
    public:
        NullRenderDevice() = default;

        [[nodiscard]] BufferHandle CreateBuffer(const BufferDesc& desc) override;
        void DestroyBuffer(BufferHandle handle) override;

        [[nodiscard]] TextureHandle CreateTexture(const TextureDesc& desc) override;
        void DestroyTexture(TextureHandle handle) override;

        void BeginRendering() override;
        bool SubmitDraw(const DrawCommand& command) override;
        void EndRendering() override;

        // Test/diagnostic inspection only. Deliberately NOT part of the
        // generic RenderDevice interface — Runtime never calls these,
        // and no other backend needs to implement them. Kept on the
        // concrete class specifically so the generic interface stays
        // free of Null-backend-specific concerns; tests construct a
        // NullRenderDevice directly (rather than going through
        // CreateNullRenderDevice, which only returns the abstract
        // interface) to reach these.
        [[nodiscard]] std::size_t GetLiveBufferCount() const;
        [[nodiscard]] std::size_t GetLiveTextureCount() const;
        [[nodiscard]] std::uint64_t GetTotalDrawCount() const;
        [[nodiscard]] std::uint32_t GetLastFrameDrawCount() const;

    private:
        std::unordered_map<std::uint32_t, BufferDesc> m_buffers;
        std::unordered_map<std::uint32_t, TextureDesc> m_textures;
        std::uint32_t m_nextBufferId = 1;
        std::uint32_t m_nextTextureId = 1;

        std::uint64_t m_totalDrawCount = 0;
        std::uint32_t m_currentFrameDrawCount = 0;
        std::uint32_t m_lastFrameDrawCount = 0;
    };

    // Constructs a NullRenderDevice behind the generic RenderDevice
    // interface. This is what Runtime actually calls — it never names
    // NullRenderDevice itself, the same way it never names
    // WindowsWindow or DesktopFrameDriver's internals. M8 will add a
    // parallel CreateVulkanRenderDevice(); there is no backend-selection
    // system yet because there is only one backend to select.
    [[nodiscard]] std::unique_ptr<RenderDevice> CreateNullRenderDevice();
}
