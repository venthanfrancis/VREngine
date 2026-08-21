// M4 automated tests for AREngine::Rendering's Null backend
// (NullRenderDevice). No human interaction, no real graphics API calls,
// no OS window — fully headless and deterministic.

#include "AREngine/Rendering/Rendering.hpp"

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

    AREngine::Rendering::BufferDesc MakeVertexBufferDesc(std::size_t sizeBytes = 12)
    {
        AREngine::Rendering::BufferDesc desc;
        desc.sizeBytes = sizeBytes;
        desc.usage = AREngine::Rendering::BufferUsage::Vertex;
        return desc;
    }

    void TestDeviceCreation()
    {
        AREngine::Rendering::NullRenderDevice device;
        Check(device.GetLiveBufferCount() == 0, "A freshly created NullRenderDevice has no live buffers");
        Check(device.GetLiveTextureCount() == 0, "A freshly created NullRenderDevice has no live textures");
        Check(device.GetTotalDrawCount() == 0, "A freshly created NullRenderDevice has submitted no draws");

        auto viaFactory = AREngine::Rendering::CreateNullRenderDevice();
        Check(viaFactory != nullptr, "CreateNullRenderDevice returns a RenderDevice");
    }

    void TestBufferHandles()
    {
        AREngine::Rendering::NullRenderDevice device;

        const auto a = device.CreateBuffer(MakeVertexBufferDesc());
        const auto b = device.CreateBuffer(MakeVertexBufferDesc());

        Check(a.IsValid(), "CreateBuffer returns a valid handle");
        Check(b.IsValid(), "CreateBuffer returns a valid handle (second buffer)");
        Check(!(a == b), "Two buffers get distinguishable handles");
        Check(device.GetLiveBufferCount() == 2, "Both created buffers are live");
    }

    void TestValidDrawIsAccepted()
    {
        AREngine::Rendering::NullRenderDevice device;
        const auto vertexBuffer = device.CreateBuffer(MakeVertexBufferDesc());

        AREngine::Rendering::DrawCommand draw;
        draw.vertexBuffer = vertexBuffer;
        draw.count = 3;

        device.BeginRendering();
        const bool accepted = device.SubmitDraw(draw);
        device.EndRendering();

        Check(accepted, "A draw referencing a valid buffer is accepted");
        Check(device.GetTotalDrawCount() == 1, "Total draw count increases by one");
        Check(device.GetLastFrameDrawCount() == 1, "Last frame's draw count is one");
    }

    void TestDrawCountAcrossFrames()
    {
        AREngine::Rendering::NullRenderDevice device;
        const auto vertexBuffer = device.CreateBuffer(MakeVertexBufferDesc());

        AREngine::Rendering::DrawCommand draw;
        draw.vertexBuffer = vertexBuffer;
        draw.count = 3;

        device.BeginRendering();
        device.SubmitDraw(draw);
        device.SubmitDraw(draw);
        device.EndRendering();

        Check(device.GetLastFrameDrawCount() == 2, "Two draws in one frame are both counted");

        device.BeginRendering();
        device.SubmitDraw(draw);
        device.EndRendering();

        Check(device.GetLastFrameDrawCount() == 1, "Last-frame count resets at the start of the next frame");
        Check(device.GetTotalDrawCount() == 3, "Total draw count accumulates across frames");
    }

    void TestInvalidHandleIsRejected()
    {
        AREngine::Rendering::NullRenderDevice device;

        AREngine::Rendering::DrawCommand draw;
        draw.vertexBuffer = AREngine::Rendering::BufferHandle{999}; // never created
        draw.count = 3;

        device.BeginRendering();
        const bool accepted = device.SubmitDraw(draw);
        device.EndRendering();

        Check(!accepted, "A draw referencing an unknown buffer handle is rejected");
        Check(device.GetTotalDrawCount() == 0, "A rejected draw does not count");

        AREngine::Rendering::DrawCommand emptyDraw; // default-constructed, invalid handle
        device.BeginRendering();
        const bool acceptedEmpty = device.SubmitDraw(emptyDraw);
        device.EndRendering();
        Check(!acceptedEmpty, "A draw with a default-constructed (invalid) handle is rejected");
    }

    void TestResourceDestruction()
    {
        AREngine::Rendering::NullRenderDevice device;
        const auto buffer = device.CreateBuffer(MakeVertexBufferDesc());
        Check(device.GetLiveBufferCount() == 1, "Buffer is live after creation");

        device.DestroyBuffer(buffer);
        Check(device.GetLiveBufferCount() == 0, "Buffer is no longer live after DestroyBuffer");

        // Double-destroy, and destroying a handle that was never
        // created, must both be predictable no-ops, not crashes.
        device.DestroyBuffer(buffer);
        device.DestroyBuffer(AREngine::Rendering::BufferHandle{12345});
        Check(device.GetLiveBufferCount() == 0, "Double-destroy and destroying an unknown handle are safe no-ops");

        AREngine::Rendering::TextureDesc textureDesc;
        textureDesc.width = 64;
        textureDesc.height = 64;
        const auto texture = device.CreateTexture(textureDesc);
        Check(device.GetLiveTextureCount() == 1, "Texture is live after creation");
        device.DestroyTexture(texture);
        Check(device.GetLiveTextureCount() == 0, "Texture is no longer live after DestroyTexture");
    }
}

int main()
{
    TestDeviceCreation();
    TestBufferHandles();
    TestValidDrawIsAccepted();
    TestDrawCountAcrossFrames();
    TestInvalidHandleIsRejected();
    TestResourceDestruction();

    if (g_failureCount == 0)
    {
        std::printf("All Rendering M4 checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
