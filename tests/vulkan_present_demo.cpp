// Manual M8G validation demo — NOT part of the automated CTest suite,
// since it requires a real Vulkan-capable GPU/driver and opens a real
// window, neither of which CI/headless systems have. Built by CMake
// but deliberately not registered with add_test. Run it manually.
//
// Extends M8F's fixed-camera depth demo with AREngine's first real
// Camera (Scene::Camera + Scene::Transform, backend-independent — see
// docs/ARCHITECTURE.md, "Camera Ownership / Module Placement (M8G)")
// and a free-fly controller (ARDemo::DemoCameraController,
// DemoCameraController.hpp — demo-private, mixes Input policy with
// motion math, deliberately NOT part of the permanent engine) driven by
// AREngine's existing InputSystem: WASD to move (following wherever the
// camera is currently looking, not always world -Z), Space/Ctrl for
// up/down, hold the right mouse button and move the mouse to look
// around. A small hard-coded scene (a floor plus several upright quads
// at different distances, including M8F's original near/far depth-proof
// pair) replaces M8F's single quad pair, so movement has real depth and
// perspective to observe. Depth testing remains enabled throughout. See
// docs/ARCHITECTURE.md, "M8G Implementation Notes" for full details.
//
// This demo reaches directly into Rendering's private src/vulkan/
// implementation (not through any public Rendering API), same as
// M8A's arengine_vulkan_demo - see docs/ARCHITECTURE.md, "Frame /
// Rendering Boundary (M8B)" for why the generic RenderDevice API is
// deliberately NOT used to drive this yet.

#include "AREngine/Core/Core.hpp"
#include "AREngine/Input/Input.hpp"
#include "AREngine/Platform/Platform.hpp"
#include "AREngine/Scene/Camera.hpp"
#include "AREngine/Scene/Transform.hpp"

#include "DemoCameraController.hpp"

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

    // One piece of M8G's hard-coded demo world: a shared quad, placed
    // and colored by its own Transform + tint. Temporary test content,
    // not a general world/level system - see docs/ARCHITECTURE.md,
    // "Demo World (M8G)".
    struct SceneObject
    {
        Scene::Transform transform;
        Core::Math::Vec4 tint;
    };

    // A floor plus several upright quads at different distances/
    // positions - enough for movement to have real depth/perspective to
    // observe, without implementing MeshAsset/model loading or a
    // general world system. Includes M8F's original near/far
    // depth-testing pair unchanged (still proves the same occlusion
    // fact — see docs/ARCHITECTURE.md, "Exact Visual Proof That Depth
    // Testing Works (M8F)" — now just two objects among several the
    // user can walk around and view from any angle).
    [[nodiscard]] std::vector<SceneObject> BuildDemoScene()
    {
        using namespace AREngine::Core::Math;
        using AREngine::Scene::Transform;

        const float halfPi = std::numbers::pi_v<float> / 2.0f;

        return {
            // Floor: local quad (facing +Z) rotated 90deg around Right
            // so its plane becomes horizontal (XZ), then scaled up into
            // a 20x20m ground plane. Scale is applied before rotation
            // (TRS order), so scaling X/Y (not Z) here is what ends up
            // stretching the horizontal plane's X/Z extent once rotated.
            {Transform{Vec3(0.0f, -1.0f, -5.0f), Quaternion::FromAxisAngle(kWorldRight, halfPi), Vec3(20.0f, 20.0f, 1.0f)},
             Vec4(1.0f, 1.0f, 1.0f, 1.0f)},

            // M8F's original depth-testing pair, positions unchanged.
            {Transform{Vec3(-2.0f, 0.5f, -3.0f), Quaternion::Identity(), Vec3(1.0f, 1.0f, 1.0f)},
             Vec4(1.0f, 1.0f, 1.0f, 1.0f)}, // near, untinted
            {Transform{Vec3(-1.6f, 0.5f, -5.0f), Quaternion::Identity(), Vec3(1.0f, 1.0f, 1.0f)},
             Vec4(1.0f, 0.35f, 0.35f, 1.0f)}, // far, red

            // A couple more, farther out, so there is real scene depth
            // to walk through and look around.
            {Transform{Vec3(2.0f, 0.5f, -6.0f), Quaternion::Identity(), Vec3(1.0f, 1.0f, 1.0f)},
             Vec4(0.4f, 1.0f, 0.4f, 1.0f)}, // green
            {Transform{Vec3(3.5f, 0.5f, -9.0f), Quaternion::Identity(), Vec3(1.0f, 1.0f, 1.0f)},
             Vec4(0.4f, 0.4f, 1.0f, 1.0f)}, // blue
        };
    }
}

int main()
{
    // Declared first, before `window`, for exactly the reason
    // Runtime.hpp documents for its own m_inputSystem: Win32's
    // DestroyWindow (reached via ~WindowsWindow, reached via ~window)
    // can synchronously fire one last WM_KILLFOCUS *during window
    // teardown*, which the event callback below forwards to
    // inputSystem.OnEvent(). If inputSystem were declared (and
    // therefore destroyed) after window, that late event would call
    // OnEvent() on an already-destroyed InputSystem — see
    // docs/ARCHITECTURE.md, "Input Integration (M8G)" and the original
    // M7 incident this reasoning comes from (runtime/include/AREngine/Runtime/Runtime.hpp).
    Input::InputSystem inputSystem;

    Platform::WindowDesc desc;
    desc.title = "AREngine M8G Vulkan Camera Demo";
    desc.width = 1280;
    desc.height = 720;
    auto window = Platform::CreateAppWindow(desc);

    bool framebufferResized = false;
    window->SetEventCallback([&framebufferResized, &inputSystem](Core::Event& event)
    {
        // Every event reaches InputSystem unconditionally — including
        // WindowFocusLostEvent, which InputSystem uses to clear all
        // held keys/buttons (see docs/ARCHITECTURE.md, "Focus Loss
        // (M8G)"). This is the one place Platform's events reach
        // InputSystem, same pattern Runtime::Runtime() already
        // established.
        inputSystem.OnEvent(event);

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

    // M8G's real Camera: backend-independent projection parameters
    // (Scene::Camera) driving a view matrix built from a Transform's
    // pose - no Vulkan type anywhere in either. near/far widened from
    // M8F's 0.1/10.0 to comfortably contain the larger demo world below
    // (the floor alone spans roughly Z in [-15,5]). See
    // docs/ARCHITECTURE.md, "Camera Data (M8G)".
    Scene::Camera camera;
    camera.nearZ = 0.1f;
    camera.farZ = 100.0f;

    // Starting pose: 1m above the floor (floor sits at y=-1), a few
    // meters back from the demo scene, facing -Z (identity rotation) -
    // i.e. looking straight into the scene, matching
    // DemoCameraController's own yaw=pitch=0 default so the very first
    // frame's orientation and this Transform agree.
    Scene::Transform cameraTransform;
    cameraTransform.position = Core::Math::Vec3(0.0f, 1.0f, 5.0f);

    ARDemo::DemoCameraController cameraController;

    // Push-constant size review (M8G): still one MvpPushConstants (80
    // bytes) per draw call, computed fresh on the CPU per object -
    // adding more scene objects didn't change the PER-DRAW payload
    // size, only how many times it's pushed. Asserted against the
    // device's actual limit rather than assumed - every Vulkan
    // implementation guarantees at least 128 bytes, comfortably above
    // 80, but this is queried/asserted, not just trusted. See
    // docs/ARCHITECTURE.md, "Push Constant Review (M8G)".
    AR_ASSERT_MSG(physicalDevice.properties.limits.maxPushConstantsSize >= sizeof(MvpPushConstants),
        "Device's maxPushConstantsSize is smaller than MvpPushConstants - should be spec-impossible (guaranteed >= 128 bytes)");

    const std::vector<SceneObject> sceneObjects = BuildDemoScene();
    AR_LOG_INFO(std::format("Demo scene: {} objects (1 floor + {} upright quads)",
                             sceneObjects.size(), sceneObjects.size() - 1));

    Platform::SteadyClock clock;

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

    AR_LOG_INFO("AREngine M8G camera demo: WASD to move, Space/Ctrl for up/down, "
                "hold Right Mouse Button and move the mouse to look around. Close the window to exit.");

    int currentFrame = 0;
    while (!window->ShouldClose())
    {
        // InputSystem::BeginFrame() must run before PollEvents()
        // delivers this frame's new events - see
        // docs/ARCHITECTURE.md, "Input Integration (M8G)" (same
        // reasoning as Runtime::Run()'s identical ordering).
        inputSystem.BeginFrame();

        window->PollEvents();
        if (window->ShouldClose())
        {
            break;
        }

        // Ticked every loop iteration, even while minimized below, so
        // elapsed wall-clock time never silently accumulates into one
        // large jump the moment the window is restored. See
        // docs/ARCHITECTURE.md, "Delta-Time Usage (M8G)".
        const float deltaTimeSeconds = static_cast<float>(clock.Tick());

        if (window->GetWidth() == 0 || window->GetHeight() == 0)
        {
            continue; // minimized - do no Vulkan (or camera) work until restored
        }

        // The only place this demo touches camera movement/look - see
        // docs/ARCHITECTURE.md, "Movement (M8G)" and "Mouse Look
        // (M8G)". A held key surviving a focus-loss/regain cycle with
        // no stuck movement is InputSystem's existing M7 behavior,
        // exercised here for the first time by something that's
        // actually visible (camera motion) rather than a log line.
        cameraController.Update(cameraTransform, inputSystem, deltaTimeSeconds);

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
        // and its index buffer, then the texture's descriptor set once
        // for the whole frame, then draw indexed once per scene object.
        // See docs/ARCHITECTURE.md, "Indexed Draw Path (M8D)", "Command
        // Recording (M8E)", and "Exact Visual Proof That Depth Testing
        // Works (M8F)".
        VkBuffer vertexBuffers[] = {vertexBuffer->Get()};
        VkDeviceSize vertexOffsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, vertexOffsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer->Get(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetLayout(),
            0, 1, &descriptorSet, 0, nullptr);

        // Aspect ratio is set from the CURRENT extent every frame -
        // this is what keeps projection correct after a resize, with no
        // separate "update projection" step anywhere else. View comes
        // straight from the camera's current Transform (moved/rotated
        // by cameraController.Update above). Core::Math::PerspectiveRH_ZO
        // (via Camera::GetProjectionMatrix) builds the backend-neutral
        // RH/zero-to-one-depth matrix; ApplyVulkanYFlip is the one,
        // explicit place Vulkan's NDC Y-flip is applied - see
        // docs/ARCHITECTURE.md, "Core/Vulkan Clip-Space Split" and
        // "Projection Ownership (M8G)".
        camera.SetAspectRatio(static_cast<float>(extent.width) / static_cast<float>(extent.height));
        const Core::Math::Mat4 view = camera.GetViewMatrix(cameraTransform);
        const Core::Math::Mat4 projection = ApplyVulkanYFlip(camera.GetProjectionMatrix());
        const Core::Math::Mat4 viewProjection = projection * view;

        // One draw call per scene object: MVP computed fresh on the CPU
        // (Model varies per object; View/Projection are this frame's,
        // shared by all of them) and pushed immediately before each
        // draw - see docs/ARCHITECTURE.md, "Push Constant Review
        // (M8G)". Still no vertex buffer of its own per object - every
        // object reuses the one shared quad bound above.
        for (const SceneObject& object : sceneObjects)
        {
            const MvpPushConstants pushConstants{viewProjection * object.transform.ToMatrix(), object.tint};
            vkCmdPushConstants(commandBuffer, pipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof(MvpPushConstants), &pushConstants);
            vkCmdDrawIndexed(commandBuffer, static_cast<std::uint32_t>(indices.size()), 1, 0, 0, 0);
        }

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
    // "Exact Destruction Order (M8B)" and "M8C"/"M8D"/"M8E"/"M8F"/"M8G
    // Implementation Notes". This also covers the vertex/index buffers,
    // the texture/descriptor resources, and the depth image: the last
    // frame's draw commands may still be referencing all of them until
    // the GPU actually finishes, which this wait guarantees. Everything
    // else (sync objects, descriptor pool, sampler, texture image,
    // index buffer, vertex buffer, command pool, depth image,
    // framebuffers, pipeline, descriptor set layout, render pass,
    // swapchain, device, surface, instance, window, inputSystem)
    // unwinds automatically after this in reverse construction order —
    // inputSystem last of all, per the ordering comment where it's
    // declared, above.
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
