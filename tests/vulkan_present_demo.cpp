// Manual M8F validation demo — NOT part of the automated CTest suite,
// since it requires a real Vulkan-capable GPU/driver and opens a real
// window, neither of which CI/headless systems have. Built by CMake
// but deliberately not registered with add_test. Run it manually.
//
// Extends M8E's textured quad into genuine 3D: real Vec3 positions, a
// fixed camera (Core::Math::LookAtRH), a right-handed/zero-to-one-depth
// perspective projection (Core::Math::PerspectiveRH_ZO) with Vulkan's
// NDC Y-flip applied as a separate, explicit Vulkan-layer step
// (VulkanClipSpace::ApplyVulkanYFlip - see docs/ARCHITECTURE.md,
// "Core/Vulkan Clip-Space Split"), a depth buffer, and depth testing -
// proven by drawing two overlapping, differently-tinted copies of the
// same quad at different depths, the nearer one submitted FIRST, and
// confirming it still renders in front. See docs/ARCHITECTURE.md,
// "Exact Visual Proof That Depth Testing Works (M8F)". No movable
// camera, no Scene integration, no lighting - see docs/ROADMAP.md, M8F.
//
// This demo reaches directly into Rendering's private src/vulkan/
// implementation (not through any public Rendering API), same as
// M8A's arengine_vulkan_demo - see docs/ARCHITECTURE.md, "Frame /
// Rendering Boundary (M8B)" for why the generic RenderDevice API is
// deliberately NOT used to drive this yet.

#include "AREngine/Core/Core.hpp"
#include "AREngine/Platform/Platform.hpp"

#include "vulkan/VulkanBuffer.hpp"
#include "vulkan/VulkanCheckerboard.hpp"
#include "vulkan/VulkanClipSpace.hpp"
#include "vulkan/VulkanCommandPool.hpp"
#include "vulkan/VulkanDepthFormat.hpp"
#include "vulkan/VulkanDescriptorPool.hpp"
#include "vulkan/VulkanDescriptorSetLayout.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanFramebuffers.hpp"
#include "vulkan/VulkanGraphicsPipeline.hpp"
#include "vulkan/VulkanImage.hpp"
#include "vulkan/VulkanInstance.hpp"
#include "vulkan/VulkanPhysicalDevice.hpp"
#include "vulkan/VulkanPushConstants.hpp"
#include "vulkan/VulkanQueueFamilies.hpp"
#include "vulkan/VulkanRenderPass.hpp"
#include "vulkan/VulkanResult.hpp"
#include "vulkan/VulkanSampler.hpp"
#include "vulkan/VulkanSurface.hpp"
#include "vulkan/VulkanSwapchain.hpp"
#include "vulkan/VulkanSwapchainSupport.hpp"
#include "vulkan/VulkanVersion.hpp"
#include "vulkan/VulkanVertex.hpp"

#include <array>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <numbers>
#include <vector>

using namespace AREngine;
using namespace AREngine::Rendering::Vulkan;

namespace
{
    // Two frames in flight: enough to let the CPU record frame N+1
    // while the GPU still works on frame N, without unbounded queued
    // work. See docs/ARCHITECTURE.md, "Synchronization Model (M8B)".
    constexpr int kMaxFramesInFlight = 2;

    // The render pass's background clear color - a visible, non-black
    // color so a human can immediately confirm presentation is working
    // and see the triangle stand out against it. AREngine blue-ish
    // teal, not anything meaningful beyond that.
    constexpr VkClearColorValue kClearColor{{0.06f, 0.30f, 0.42f, 1.0f}};

    // Per frame-in-flight: waited/signaled by that frame's own
    // acquire+submit, so reuse is bounded purely by `inFlight`'s fence
    // - safe regardless of image count.
    struct FrameSyncObjects
    {
        VkSemaphore imageAvailable = VK_NULL_HANDLE;
        VkFence inFlight = VK_NULL_HANDLE;
    };

    VkSemaphore CreateSemaphore(VkDevice device)
    {
        VkSemaphoreCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkSemaphore semaphore = VK_NULL_HANDLE;
        CheckVkResult(vkCreateSemaphore(device, &createInfo, nullptr, &semaphore), "vkCreateSemaphore");
        return semaphore;
    }

    VkFence CreateFence(VkDevice device, bool signaled)
    {
        VkFenceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        createInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
        VkFence fence = VK_NULL_HANDLE;
        CheckVkResult(vkCreateFence(device, &createInfo, nullptr, &fence), "vkCreateFence");
        return fence;
    }
}

int main()
{
    Platform::WindowDesc desc;
    desc.title = "AREngine M8F Vulkan Depth Demo";
    desc.width = 1280;
    desc.height = 720;
    auto window = Platform::CreateAppWindow(desc);

    bool framebufferResized = false;
    window->SetEventCallback([&framebufferResized](Core::Event& event)
    {
        if (dynamic_cast<Platform::WindowResizeEvent*>(&event) != nullptr)
        {
            framebufferResized = true;
        }
    });

    VulkanInstance instance(/*enablePresentationExtensions=*/true);
    AR_LOG_INFO(instance.IsValidationEnabled()
                    ? "Validation layer enabled (VK_LAYER_KHRONOS_validation)"
                    : "Validation layer NOT enabled (unavailable, or a Release build)");

    VulkanSurface surface(instance.Get(), window->GetNativeHandle());

    const SelectedPresentableDevice physicalDevice = SelectPhysicalDeviceForPresentation(instance.Get(), surface.Get());
    AR_LOG_INFO(std::format("Selected GPU: {} ({})",
                             physicalDevice.properties.deviceName,
                             PhysicalDeviceTypeToString(physicalDevice.properties.deviceType)));
    AR_LOG_INFO(std::format("Graphics queue family index: {}", physicalDevice.queueFamilies.graphicsFamily));
    AR_LOG_INFO(std::format("Present queue family index: {}", physicalDevice.queueFamilies.presentFamily));
    AR_LOG_INFO(HasSeparatePresentQueue(physicalDevice.queueFamilies)
                    ? "Graphics and present use DIFFERENT queue families"
                    : "Graphics and present share the SAME queue family");

    VulkanDevice device(physicalDevice.device, physicalDevice.queueFamilies, /*enableSwapchainExtension=*/true);

    auto swapchain = std::make_unique<VulkanSwapchain>(
        physicalDevice.device, device.Get(), surface.Get(), physicalDevice.queueFamilies,
        window->GetWidth(), window->GetHeight());
    AR_LOG_INFO(std::format("Swapchain image format: {}", static_cast<int>(swapchain->GetImageFormat())));
    AR_LOG_INFO(std::format("Swapchain extent: {}x{}", swapchain->GetExtent().width, swapchain->GetExtent().height));
    AR_LOG_INFO(std::format("Swapchain image count: {}", swapchain->GetImageCount()));

    // Chosen once, from device capabilities - fixed for the whole
    // demo's lifetime, same as the swapchain's color format. See
    // docs/ARCHITECTURE.md, "Depth Format Selection (M8F)".
    const VkFormat depthFormat = FindSupportedDepthFormat(physicalDevice.device);
    AR_LOG_INFO(std::format("Depth format: {}", static_cast<int>(depthFormat)));

    // renderPass/pipeline depend only on the swapchain's color FORMAT
    // and the (also fixed) depth format, neither of which changes
    // across a resize - so, unlike the swapchain itself, neither needs
    // to be recreated in recreateSwapchain() below. Only framebuffers
    // and the depth image itself (both extent-dependent) do. See
    // docs/ARCHITECTURE.md, "Swapchain-Dependent Pipeline Resources
    // (M8C)" and "Depth Lifetime (M8F)".
    VulkanRenderPass renderPass(device.Get(), swapchain->GetImageFormat(), depthFormat);

    // One combined-image-sampler binding, matching triangle.frag's
    // `layout(set = 0, binding = 0) uniform sampler2D uTexture`. See
    // docs/ARCHITECTURE.md, "Descriptor Set Layout (M8E)".
    VulkanDescriptorSetLayout descriptorSetLayout(device.Get());

    VulkanGraphicsPipeline pipeline(device.Get(), renderPass.Get(), descriptorSetLayout.Get());

    // The depth image IS swapchain/extent-dependent (unlike a texture -
    // see docs/ARCHITECTURE.md, "Depth Lifetime (M8F)"): sized to the
    // swapchain's current extent, so it must be destroyed and rebuilt
    // alongside the swapchain on every resize, same as framebuffers.
    auto depthImage = std::make_unique<VulkanImage>(
        physicalDevice.device, device.Get(), swapchain->GetExtent().width, swapchain->GetExtent().height,
        depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT);

    auto framebuffers = std::make_unique<VulkanFramebuffers>(
        device.Get(), renderPass.Get(), swapchain->GetImageViews(), depthImage->GetView(), swapchain->GetExtent());

    VulkanCommandPool commandPool(device.Get(), physicalDevice.queueFamilies.graphicsFamily);

    // One shared quad (4 vertices, 6 indices, 2 triangles), drawn TWICE
    // per frame at two different depths via two different Model
    // matrices (see the fixed camera/depth-proof setup below) - proves
    // both vertex-buffer and index-buffer infrastructure (a triangle
    // alone wouldn't need an index buffer to be meaningful). Neither
    // buffer is swapchain-dependent - both are created once here and
    // survive every swapchain recreation untouched. See
    // docs/ARCHITECTURE.md, "Resource Lifetime: Swapchain-Dependent vs.
    // Geometry (M8D)".
    //
    //   0 ---- 1
    //   |    / |
    //   |   /  |
    //   |  /   |
    //   | /    |
    //   3 ---- 2
    //
    // Positions are local/object-space (z=0 - depth comes entirely from
    // the Model matrix's translation, not baked into the vertex data;
    // see docs/ARCHITECTURE.md, "Vec3 Vertex Layout (M8F)"). UVs map
    // each corner to the matching corner of the texture (0,0) top-left
    // to (1,1) bottom-right - the same corner order as position, so the
    // whole texture covers the whole quad exactly once, with no
    // cropping or repetition.
    const std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // 0
        {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}}, // 1
        {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // 2
        {{-0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}, // 3
    };
    const std::vector<std::uint32_t> indices = {0, 1, 2, 2, 3, 0};

    auto vertexBuffer = CreateDeviceLocalBuffer(
        physicalDevice.device, device.Get(), commandPool.Get(), device.GetGraphicsQueue(),
        vertices.data(), sizeof(Vertex) * vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    auto indexBuffer = CreateDeviceLocalBuffer(
        physicalDevice.device, device.Get(), commandPool.Get(), device.GetGraphicsQueue(),
        indices.data(), sizeof(std::uint32_t) * indices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    AR_LOG_INFO(std::format("Vertex buffer: {} vertices, {} bytes, stride {} bytes",
                             vertices.size(), vertexBuffer->GetSize(), sizeof(Vertex)));
    AR_LOG_INFO(std::format("Index buffer: {} indices (uint32_t), {} bytes",
                             indices.size(), indexBuffer->GetSize()));

    // A tiny procedural checkerboard - M8E's whole "texture asset",
    // deliberately generated rather than decoded from a file (no PNG/
    // JPEG support exists yet). Not swapchain-dependent - created once,
    // survives every resize untouched, same as the vertex/index
    // buffers above. See docs/ARCHITECTURE.md, "Resource Lifetime
    // (M8E)".
    constexpr std::uint32_t kTextureWidth = 64;
    constexpr std::uint32_t kTextureHeight = 64;
    constexpr std::uint32_t kTextureTileSize = 8;
    const std::vector<std::uint8_t> checkerboardPixels =
        GenerateCheckerboardRGBA8(kTextureWidth, kTextureHeight, kTextureTileSize);

    auto texture = CreateTextureFromPixels(
        physicalDevice.device, device.Get(), commandPool.Get(), device.GetGraphicsQueue(),
        kTextureWidth, kTextureHeight, checkerboardPixels.data());
    VulkanSampler sampler(device.Get());
    AR_LOG_INFO(std::format("Texture: {}x{} VK_FORMAT_R8G8B8A8_SRGB, {} bytes, {}x{} tile checkerboard",
                             kTextureWidth, kTextureHeight, checkerboardPixels.size(),
                             kTextureTileSize, kTextureTileSize));

    // One pool, one set, one write - see docs/ARCHITECTURE.md,
    // "Descriptor Pool/Set Ownership (M8E)".
    VulkanDescriptorPool descriptorPool(device.Get());
    VkDescriptorSet descriptorSet = descriptorPool.Allocate(descriptorSetLayout.Get());
    WriteCombinedImageSamplerDescriptor(device.Get(), descriptorSet, texture->GetView(), sampler.Get());

    // M8F's fixed camera: positioned 3 meters back along +Z, looking
    // toward the origin - i.e. looking down world -Z, exactly matching
    // AREngine's Forward convention. No keyboard/mouse control, no
    // Camera component - see docs/ARCHITECTURE.md, "Camera Convention
    // (M8F)". View never changes once computed; projection is
    // recomputed every frame from the swapchain's *current* extent
    // (see the render loop below), so a resize naturally updates the
    // aspect ratio with no separate "update projection" step.
    const Core::Math::Vec3 kCameraEye(0.0f, 0.0f, 3.0f);
    const Core::Math::Mat4 view = Core::Math::LookAtRH(kCameraEye, Core::Math::Vec3(0.0f, 0.0f, 0.0f), Core::Math::kWorldUp);

    constexpr float kFovYRadians = std::numbers::pi_v<float> / 3.0f; // 60 degrees
    constexpr float kNearZ = 0.1f;
    constexpr float kFarZ = 10.0f;

    // The exact depth-testing proof (see docs/ARCHITECTURE.md, "Exact
    // Visual Proof That Depth Testing Works (M8F)"): two copies of the
    // same quad, placed at different world-space depths AND offset
    // diagonally in X/Y, purely via their Model matrix's translation
    // (the shared vertex data above never changes) - so the two quads
    // partially, not fully, overlap on screen. This deliberately makes
    // the proof unambiguous from a single screenshot: each quad also
    // has a non-overlapping region that's independently visible
    // regardless of depth testing, so a viewer can directly compare
    // "what the overlap region shows" against "what each quad's own
    // color looks like elsewhere" - a full nested overlap (same X/Y,
    // only Z different) would make it impossible to tell the far quad
    // was ever drawn at all.
    //
    // The near quad (z=-1.5, tinted white - i.e. untinted) is submitted
    // FIRST; the far quad (offset to x=y=+0.4, z=-2.0, tinted red) is
    // submitted SECOND. Without depth testing, the far quad's later
    // draw would incorrectly paint its red tint over the near quad in
    // the overlapping region; with depth testing (LESS, write-enabled),
    // the near quad's already-written, smaller depth values correctly
    // reject the far quad's fragments there instead - the overlap
    // region must show near's untinted checkerboard, not far's red one.
    const Core::Math::Mat4 modelNear = Core::Math::Mat4::Translation(Core::Math::Vec3(0.0f, 0.0f, -1.5f));
    const Core::Math::Mat4 modelFar = Core::Math::Mat4::Translation(Core::Math::Vec3(0.4f, 0.4f, -2.0f));
    constexpr Core::Math::Vec4 kNearTint(1.0f, 1.0f, 1.0f, 1.0f);
    constexpr Core::Math::Vec4 kFarTint(1.0f, 0.35f, 0.35f, 1.0f);

    std::array<VkCommandBuffer, kMaxFramesInFlight> commandBuffers{};
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool.Get();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = kMaxFramesInFlight;
        CheckVkResult(vkAllocateCommandBuffers(device.Get(), &allocInfo, commandBuffers.data()), "vkAllocateCommandBuffers");
    }

    std::array<FrameSyncObjects, kMaxFramesInFlight> frameSync{};
    for (FrameSyncObjects& sync : frameSync)
    {
        sync.imageAvailable = CreateSemaphore(device.Get());
        sync.inFlight = CreateFence(device.Get(), /*signaled=*/true);
    }

    // Tracks, per swapchain image (not per frame-in-flight), the fence
    // of whichever in-flight frame is currently using it — the
    // standard technique to avoid a rare race when frames-in-flight <
    // swapchain image count. VK_NULL_HANDLE means "not in use yet".
    // See docs/ARCHITECTURE.md, "Synchronization Model (M8B)".
    std::vector<VkFence> imagesInFlight(swapchain->GetImageCount(), VK_NULL_HANDLE);

    // renderFinished is indexed by swapchain IMAGE index, not frame-
    // in-flight index, and lives exactly as long as the swapchain
    // does. This is deliberate, not just tidiness: the presentation
    // engine acquires images in an order that isn't guaranteed to walk
    // frame-in-flight order 1:1 (found the hard way, as a real
    // validation error — VUID-vkQueueSubmit-pSignalSemaphores-00067 —
    // when M8B was first run against real hardware with a 2-deep
    // frames-in-flight but a 3-image swapchain). Indexing by
    // frame-in-flight let a semaphore still be in use by a pending
    // present when it was signaled again for a different image. See
    // docs/ARCHITECTURE.md, "Synchronization Model (M8B)".
    auto createRenderFinishedSemaphores = [&](std::uint32_t count)
    {
        std::vector<VkSemaphore> semaphores(count);
        for (VkSemaphore& semaphore : semaphores)
        {
            semaphore = CreateSemaphore(device.Get());
        }
        return semaphores;
    };
    std::vector<VkSemaphore> renderFinishedSemaphores = createRenderFinishedSemaphores(swapchain->GetImageCount());

    auto recreateSwapchain = [&]()
    {
        // Minimize handling: wait until the surface itself would
        // produce a non-zero extent, not just until AREngine's own
        // Window reports a non-zero width/height. Those two can
        // disagree for a moment around a minimize/restore transition —
        // Window's cached size can still read as the pre-minimize
        // size while the OS-level surface capabilities already report
        // a degenerate {0,0} currentExtent (found the hard way, as a
        // real vkCreateSwapchainKHR validation error, when M8B was
        // first run against real hardware and minimized/restored).
        // Querying the surface directly, the same way the swapchain
        // itself will, is the only way to be sure. See
        // docs/ARCHITECTURE.md, "Minimize Handling (M8B)".
        while (!window->ShouldClose())
        {
            const SwapchainSupportDetails support = QuerySwapchainSupport(physicalDevice.device, surface.Get());
            const VkExtent2D extent = ChooseSwapchainExtent(support.capabilities, window->GetWidth(), window->GetHeight());
            if (extent.width != 0 && extent.height != 0)
            {
                break;
            }
            window->PollEvents();
        }
        if (window->ShouldClose())
        {
            return;
        }

        vkDeviceWaitIdle(device.Get());

        // Framebuffers before swapchain/depth image: they wrap the
        // swapchain's image views AND the depth image's view, neither
        // of which may be destroyed while a framebuffer built from them
        // still exists. See docs/ARCHITECTURE.md, "Swapchain-Dependent
        // Pipeline Resources (M8C)" and "Depth Lifetime (M8F)".
        framebuffers.reset();
        depthImage.reset();
        swapchain.reset(); // destroy-before-construct: see VulkanSwapchain.hpp
        swapchain = std::make_unique<VulkanSwapchain>(
            physicalDevice.device, device.Get(), surface.Get(), physicalDevice.queueFamilies,
            window->GetWidth(), window->GetHeight());
        // Depth image is extent-dependent (unlike a texture) - rebuilt
        // at the swapchain's new extent every time. See
        // docs/ARCHITECTURE.md, "Depth Lifetime (M8F)".
        depthImage = std::make_unique<VulkanImage>(
            physicalDevice.device, device.Get(), swapchain->GetExtent().width, swapchain->GetExtent().height,
            depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT);
        framebuffers = std::make_unique<VulkanFramebuffers>(
            device.Get(), renderPass.Get(), swapchain->GetImageViews(), depthImage->GetView(), swapchain->GetExtent());

        imagesInFlight.assign(swapchain->GetImageCount(), VK_NULL_HANDLE);

        for (VkSemaphore semaphore : renderFinishedSemaphores)
        {
            vkDestroySemaphore(device.Get(), semaphore, nullptr);
        }
        renderFinishedSemaphores = createRenderFinishedSemaphores(swapchain->GetImageCount());

        AR_LOG_INFO(std::format("Swapchain recreated: {}x{}, {} images",
                                 swapchain->GetExtent().width, swapchain->GetExtent().height, swapchain->GetImageCount()));
    };

    AR_LOG_INFO("AREngine M8F depth demo: close the window to exit.");

    int currentFrame = 0;
    while (!window->ShouldClose())
    {
        window->PollEvents();
        if (window->ShouldClose())
        {
            break;
        }
        if (window->GetWidth() == 0 || window->GetHeight() == 0)
        {
            continue; // minimized - do no Vulkan work until restored
        }

        vkWaitForFences(device.Get(), 1, &frameSync[currentFrame].inFlight, VK_TRUE, std::numeric_limits<std::uint64_t>::max());

        std::uint32_t imageIndex = 0;
        const VkResult acquireResult = vkAcquireNextImageKHR(
            device.Get(), swapchain->Get(), std::numeric_limits<std::uint64_t>::max(),
            frameSync[currentFrame].imageAvailable, VK_NULL_HANDLE, &imageIndex);

        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreateSwapchain();
            continue;
        }
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
        {
            CheckVkResult(acquireResult, "vkAcquireNextImageKHR");
        }

        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE)
        {
            vkWaitForFences(device.Get(), 1, &imagesInFlight[imageIndex], VK_TRUE, std::numeric_limits<std::uint64_t>::max());
        }
        imagesInFlight[imageIndex] = frameSync[currentFrame].inFlight;

        VkCommandBuffer commandBuffer = commandBuffers[currentFrame];
        vkResetCommandBuffer(commandBuffer, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        CheckVkResult(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer");

        const VkExtent2D extent = swapchain->GetExtent();

        // Background clear + the layout transitions into and out of
        // COLOR_ATTACHMENT_OPTIMAL/PRESENT_SRC_KHR all happen as part
        // of the render pass itself now (loadOp=CLEAR, the attachment's
        // initial/finalLayout - see VulkanRenderPass.cpp) - M8B's
        // separate vkCmdClearColorImage + manual barrier pair is gone;
        // there is exactly one clear path now, not two competing ones.
        // Depth is cleared to 1.0 every frame too - the "farthest
        // possible" value, paired with depthCompareOp=LESS (see
        // docs/ARCHITECTURE.md, "Depth Compare / Clear Values (M8F)").
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = kClearColor;
        clearValues[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo renderPassBegin{};
        renderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBegin.renderPass = renderPass.Get();
        renderPassBegin.framebuffer = framebuffers->Get(imageIndex);
        renderPassBegin.renderArea.offset = {0, 0};
        renderPassBegin.renderArea.extent = extent;
        renderPassBegin.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
        renderPassBegin.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

        // Viewport/scissor are dynamic pipeline state (see
        // VulkanGraphicsPipeline.cpp), set fresh every frame from the
        // current swapchain extent - this is what lets the pipeline
        // itself stay valid across a resize instead of needing
        // recreation. See docs/ARCHITECTURE.md, "Viewport / Scissor
        // Strategy (M8C)".
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = extent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.Get());

        // Real geometry from real GPU buffers: bind the quad's vertex
        // buffer at binding 0 (matching Vertex::GetBindingDescription())
        // and its index buffer, then the texture's descriptor set, then
        // draw indexed - twice, once per depth-proof object. See
        // docs/ARCHITECTURE.md, "Indexed Draw Path (M8D)", "Command
        // Recording (M8E)", and "Exact Visual Proof That Depth Testing
        // Works (M8F)".
        VkBuffer vertexBuffers[] = {vertexBuffer->Get()};
        VkDeviceSize vertexOffsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, vertexOffsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer->Get(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetLayout(),
            0, 1, &descriptorSet, 0, nullptr);

        // Projection is recomputed every frame from the CURRENT extent
        // - this is what keeps the aspect ratio correct after a resize,
        // with no separate "update projection" step anywhere else. See
        // docs/ARCHITECTURE.md, "Vulkan Projection Convention (M8F)".
        // Core::Math::PerspectiveRH_ZO builds the backend-neutral RH/
        // zero-to-one-depth matrix; ApplyVulkanYFlip is the one,
        // explicit place Vulkan's NDC Y-flip is applied - see
        // docs/ARCHITECTURE.md, "Core/Vulkan Clip-Space Split".
        const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
        const Core::Math::Mat4 projection =
            ApplyVulkanYFlip(Core::Math::PerspectiveRH_ZO(kFovYRadians, aspect, kNearZ, kFarZ));

        // Near quad FIRST, far quad SECOND - the exact ordering the
        // depth-testing proof depends on (see the comment where
        // modelNear/modelFar are defined, above).
        const MvpPushConstants nearPushConstants{projection * view * modelNear, kNearTint};
        vkCmdPushConstants(commandBuffer, pipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(MvpPushConstants), &nearPushConstants);
        vkCmdDrawIndexed(commandBuffer, static_cast<std::uint32_t>(indices.size()), 1, 0, 0, 0);

        const MvpPushConstants farPushConstants{projection * view * modelFar, kFarTint};
        vkCmdPushConstants(commandBuffer, pipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(MvpPushConstants), &farPushConstants);
        vkCmdDrawIndexed(commandBuffer, static_cast<std::uint32_t>(indices.size()), 1, 0, 0, 0);

        vkCmdEndRenderPass(commandBuffer);

        CheckVkResult(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");

        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &frameSync[currentFrame].imageAvailable;
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderFinishedSemaphores[imageIndex];

        vkResetFences(device.Get(), 1, &frameSync[currentFrame].inFlight);
        CheckVkResult(vkQueueSubmit(device.GetGraphicsQueue(), 1, &submitInfo, frameSync[currentFrame].inFlight), "vkQueueSubmit");

        VkSwapchainKHR swapchainHandle = swapchain->Get();
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinishedSemaphores[imageIndex];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchainHandle;
        presentInfo.pImageIndices = &imageIndex;

        const VkResult presentResult = vkQueuePresentKHR(device.GetPresentQueue(), &presentInfo);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || framebufferResized)
        {
            framebufferResized = false;
            recreateSwapchain();
        }
        else
        {
            CheckVkResult(presentResult, "vkQueuePresentKHR");
        }

        currentFrame = (currentFrame + 1) % kMaxFramesInFlight;
    }

    // GPU idle before destroying anything - see docs/ARCHITECTURE.md,
    // "Exact Destruction Order (M8B)" and "M8C"/"M8D"/"M8E"/"M8F
    // Implementation Notes". This also covers the vertex/index buffers,
    // the texture/descriptor resources, and the depth image: the last
    // frame's draw commands may still be referencing all of them until
    // the GPU actually finishes, which this wait guarantees. Everything
    // else (sync objects, descriptor pool, sampler, texture image,
    // index buffer, vertex buffer, command pool, depth image,
    // framebuffers, pipeline, descriptor set layout, render pass,
    // swapchain, device, surface, instance) unwinds automatically after
    // this in reverse construction order.
    vkDeviceWaitIdle(device.Get());

    for (FrameSyncObjects& sync : frameSync)
    {
        vkDestroySemaphore(device.Get(), sync.imageAvailable, nullptr);
        vkDestroyFence(device.Get(), sync.inFlight, nullptr);
    }
    for (VkSemaphore semaphore : renderFinishedSemaphores)
    {
        vkDestroySemaphore(device.Get(), semaphore, nullptr);
    }

    AR_LOG_INFO("Vulkan presentation demo complete - shutting down");
    return 0;
}
