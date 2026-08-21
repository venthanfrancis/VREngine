#pragma once

#include "AREngine/Rendering/BufferDesc.hpp"
#include "AREngine/Rendering/DrawCommand.hpp"
#include "AREngine/Rendering/Handles.hpp"
#include "AREngine/Rendering/TextureDesc.hpp"

namespace AREngine::Rendering
{
    // The minimum abstraction over "a GPU," kept intentionally small —
    // see docs/ARCHITECTURE.md, "M4 Implementation Notes". Public
    // headers here expose zero backend-specific types (no VkBuffer,
    // ID3D12Resource, or similar) — only engine-owned handles and
    // descriptors. This interface knows nothing about gameplay, Scene,
    // Win32, OpenXR, physics, or audio.
    //
    // NullRenderDevice (this module) implements this without any
    // graphics API. A future Vulkan backend (M8) implements the same
    // interface.
    //
    // NOT this interface's concern: frame lifecycle or presentation.
    // BeginRendering/EndRendering below only bracket one frame's worth
    // of draw submissions — they are not a Present() call, and this
    // interface still has no notion of presenting to a screen. See
    // docs/ARCHITECTURE.md, "RHI Presentation".
    class RenderDevice
    {
    public:
        virtual ~RenderDevice() = default;

        [[nodiscard]] virtual BufferHandle CreateBuffer(const BufferDesc& desc) = 0;
        virtual void DestroyBuffer(BufferHandle handle) = 0;

        [[nodiscard]] virtual TextureHandle CreateTexture(const TextureDesc& desc) = 0;
        virtual void DestroyTexture(TextureHandle handle) = 0;

        // Brackets one frame's worth of draw submissions. Exists so a
        // backend can group/count draws per frame, and so callers (like
        // Runtime) have one clear, backend-neutral place to plug
        // rendering into their existing loop.
        virtual void BeginRendering() = 0;

        // Submits one draw. Returns false (and submits nothing) if
        // `command` references an unknown resource handle — a normal,
        // checkable outcome, not a fatal error.
        virtual bool SubmitDraw(const DrawCommand& command) = 0;

        virtual void EndRendering() = 0;
    };
}
