// Manual M8D validation demo — NOT part of the automated CTest suite,
// since it requires a real Vulkan-capable GPU/driver and opens a real
// window, neither of which CI/headless systems have. Built by CMake
// but deliberately not registered with add_test. Run it manually.
//
// Extends M8C's graphics pipeline with real GPU vertex/index buffers:
// AREngine Window -> VkSurfaceKHR -> presentation-capable physical
// device -> logical device with graphics+present queues -> swapchain
// -> acquire -> [render pass: clear background, draw a quad from a
// real vertex buffer + index buffer via vkCmdDrawIndexed] -> present,
// repeated every frame until the window closes, handling resize/
// minimize along the way. M8C's gl_VertexIndex-generated positions are
// gone - see docs/ARCHITECTURE.md, "Vertex Buffer (M8D)" and "Index
// Buffer (M8D)". No mesh/material/texture/camera/depth - see
// docs/ROADMAP.md, M8D.
//
// This demo reaches directly into Rendering's private src/vulkan/
// implementation (not through any public Rendering API), same as
// M8A's arengine_vulkan_demo - see docs/ARCHITECTURE.md, "Frame /
// Rendering Boundary (M8B)" for why the generic RenderDevice API is
// deliberately NOT used to drive this yet.

#include "AREngine/Core/Core.hpp"
#include "AREngine/Platform/Platform.hpp"

#include "vulkan/VulkanBuffer.hpp"
#include "vulkan/VulkanCommandPool.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanFramebuffers.hpp"
#include "vulkan/VulkanGraphicsPipeline.hpp"
#include "vulkan/VulkanInstance.hpp"
#include "vulkan/VulkanPhysicalDevice.hpp"
#include "vulkan/VulkanQueueFamilies.hpp"
#include "vulkan/VulkanRenderPass.hpp"
#include "vulkan/VulkanResult.hpp"
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
    desc.title = "AREngine M8D Vulkan Vertex Buffer Demo";
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

    // renderPass/pipeline depend only on the swapchain's FORMAT, which
    // doesn't change across a resize - so, unlike the swapchain itself,
    // neither needs to be recreated in recreateSwapchain() below. Only
    // framebuffers (which wrap image views + extent) does. See
    // docs/ARCHITECTURE.md, "Swapchain-Dependent Pipeline Resources
    // (M8C)".
    VulkanRenderPass renderPass(device.Get(), swapchain->GetImageFormat());
    VulkanGraphicsPipeline pipeline(device.Get(), renderPass.Get());

    auto framebuffers = std::make_unique<VulkanFramebuffers>(
        device.Get(), renderPass.Get(), swapchain->GetImageViews(), swapchain->GetExtent());

    VulkanCommandPool commandPool(device.Get(), physicalDevice.queueFamilies.graphicsFamily);

    // A colored quad: 4 vertices, 6 indices, 2 triangles - proves both
    // vertex-buffer and index-buffer infrastructure (a triangle alone
    // wouldn't need an index buffer to be meaningful). Neither buffer
    // is swapchain-dependent - both are created once here and survive
    // every swapchain recreation untouched. See docs/ARCHITECTURE.md,
    // "Resource Lifetime: Swapchain-Dependent vs. Geometry (M8D)".
    //
    //   0 ---- 1
    //   |    / |
    //   |   /  |
    //   |  /   |
    //   | /    |
    //   3 ---- 2
    const std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // 0
        {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, // 1
        {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, // 2
        {{-0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}}, // 3
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

        // Framebuffers before swapchain: they wrap the swapchain's
        // image views, which must not be destroyed while a framebuffer
        // built from them still exists. See docs/ARCHITECTURE.md,
        // "Swapchain-Dependent Pipeline Resources (M8C)".
        framebuffers.reset();
        swapchain.reset(); // destroy-before-construct: see VulkanSwapchain.hpp
        swapchain = std::make_unique<VulkanSwapchain>(
            physicalDevice.device, device.Get(), surface.Get(), physicalDevice.queueFamilies,
            window->GetWidth(), window->GetHeight());
        framebuffers = std::make_unique<VulkanFramebuffers>(
            device.Get(), renderPass.Get(), swapchain->GetImageViews(), swapchain->GetExtent());

        imagesInFlight.assign(swapchain->GetImageCount(), VK_NULL_HANDLE);

        for (VkSemaphore semaphore : renderFinishedSemaphores)
        {
            vkDestroySemaphore(device.Get(), semaphore, nullptr);
        }
        renderFinishedSemaphores = createRenderFinishedSemaphores(swapchain->GetImageCount());

        AR_LOG_INFO(std::format("Swapchain recreated: {}x{}, {} images",
                                 swapchain->GetExtent().width, swapchain->GetExtent().height, swapchain->GetImageCount()));
    };

    AR_LOG_INFO("AREngine M8D vertex/index buffer demo: close the window to exit.");

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
        VkClearValue clearValue{};
        clearValue.color = kClearColor;

        VkRenderPassBeginInfo renderPassBegin{};
        renderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBegin.renderPass = renderPass.Get();
        renderPassBegin.framebuffer = framebuffers->Get(imageIndex);
        renderPassBegin.renderArea.offset = {0, 0};
        renderPassBegin.renderArea.extent = extent;
        renderPassBegin.clearValueCount = 1;
        renderPassBegin.pClearValues = &clearValue;

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
        // and its index buffer, then draw indexed. See
        // docs/ARCHITECTURE.md, "Indexed Draw Path (M8D)".
        VkBuffer vertexBuffers[] = {vertexBuffer->Get()};
        VkDeviceSize vertexOffsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, vertexOffsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer->Get(), 0, VK_INDEX_TYPE_UINT32);
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
    // "Exact Destruction Order (M8B)" and "M8C"/"M8D Implementation
    // Notes". This also covers the vertex/index buffers: the last
    // frame's draw commands may still be referencing them until the
    // GPU actually finishes, which this wait guarantees. Everything
    // else (sync objects, index buffer, vertex buffer, command pool,
    // framebuffers, pipeline, render pass, swapchain, device, surface,
    // instance) unwinds automatically after this in reverse
    // construction order.
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
