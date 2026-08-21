#include "AREngine/Rendering/NullRenderDevice.hpp"

#include "AREngine/Core/Assert.hpp"
#include "AREngine/Core/Log.hpp"

namespace AREngine::Rendering
{
    BufferHandle NullRenderDevice::CreateBuffer(const BufferDesc& desc)
    {
        // A zero-size buffer is a caller bug, not a runtime condition
        // worth handling gracefully — hence an assert, not a rejection
        // like SubmitDraw's unknown-handle case below.
        AR_ASSERT_MSG(desc.sizeBytes > 0, "Buffer size must be greater than zero");

        const std::uint32_t id = m_nextBufferId++;
        m_buffers.emplace(id, desc);
        return BufferHandle{id};
    }

    void NullRenderDevice::DestroyBuffer(BufferHandle handle)
    {
        // Destroying an invalid or already-destroyed handle is a
        // predictable no-op, matching how e.g. delete nullptr behaves —
        // not an error. See docs/ARCHITECTURE.md, "Resource
        // Destruction".
        m_buffers.erase(handle.id);
    }

    TextureHandle NullRenderDevice::CreateTexture(const TextureDesc& desc)
    {
        AR_ASSERT_MSG(desc.width > 0 && desc.height > 0, "Texture dimensions must be greater than zero");

        const std::uint32_t id = m_nextTextureId++;
        m_textures.emplace(id, desc);
        return TextureHandle{id};
    }

    void NullRenderDevice::DestroyTexture(TextureHandle handle)
    {
        m_textures.erase(handle.id);
    }

    void NullRenderDevice::BeginRendering()
    {
        m_currentFrameDrawCount = 0;
    }

    bool NullRenderDevice::SubmitDraw(const DrawCommand& command)
    {
        // Unlike CreateBuffer's zero-size check above, an unknown handle
        // here is treated as a normal, checkable outcome rather than an
        // assert — a stale or mistyped handle reaching SubmitDraw is
        // plausible in real use, and rejecting it predictably (returning
        // false, logging, not crashing) is more useful than aborting.
        if (!command.vertexBuffer.IsValid() || !m_buffers.contains(command.vertexBuffer.id))
        {
            AR_LOG_WARNING("NullRenderDevice::SubmitDraw rejected: unknown or invalid vertex buffer handle");
            return false;
        }

        if (command.indexBuffer.IsValid() && !m_buffers.contains(command.indexBuffer.id))
        {
            AR_LOG_WARNING("NullRenderDevice::SubmitDraw rejected: unknown index buffer handle");
            return false;
        }

        ++m_totalDrawCount;
        ++m_currentFrameDrawCount;
        return true;
    }

    void NullRenderDevice::EndRendering()
    {
        m_lastFrameDrawCount = m_currentFrameDrawCount;
    }

    std::size_t NullRenderDevice::GetLiveBufferCount() const
    {
        return m_buffers.size();
    }

    std::size_t NullRenderDevice::GetLiveTextureCount() const
    {
        return m_textures.size();
    }

    std::uint64_t NullRenderDevice::GetTotalDrawCount() const
    {
        return m_totalDrawCount;
    }

    std::uint32_t NullRenderDevice::GetLastFrameDrawCount() const
    {
        return m_lastFrameDrawCount;
    }

    std::unique_ptr<RenderDevice> CreateNullRenderDevice()
    {
        return std::make_unique<NullRenderDevice>();
    }
}
