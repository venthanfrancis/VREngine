// Manual M12 validation demo — NOT part of the automated CTest suite,
// since it requires a real Vulkan-capable GPU/driver and opens a real
// window, neither of which CI/headless systems have. Built by CMake but
// deliberately not registered with add_test. Run it manually.
//
// AREngine's first desktop demo whose scene content is real
// AREngine::Scene::Scene data, not a hand-rolled std::vector<DemoObject>
// (contrast with vulkan_present_demo.cpp, deliberately left unmodified
// as a stable regression baseline). Reuses M8G's DemoCameraController
// for camera movement (not the milestone's concern - already proven) and
// the same window/device/swapchain/pipeline/texture bring-up
// vulkan_present_demo.cpp already established; the only thing genuinely
// new here is the Scene -> ExtractRenderables -> BuildDrawPlan ->
// DrawPlannedInstances path (tests/RenderDrawPlanning.hpp,
// tests/MeshRegistry.hpp), shared unchanged with tests/xr_demo.cpp's own
// per-view render loop. Scene content itself comes from
// tests/PopulateDemoScene.hpp, the SAME function xr_demo.cpp calls - one
// scene, two presentation paths, no duplicated world data. See
// docs/ARCHITECTURE.md, "M12 - Renderable Scene Integration Foundation".

#include "AREngine/Core/Core.hpp"
#include "AREngine/Input/Input.hpp"
#include "AREngine/Platform/Platform.hpp"
#include "AREngine/Rendering/ProceduralMesh.hpp"
#include "AREngine/Scene/Camera.hpp"
#include "AREngine/Scene/Scene.hpp"
#include "AREngine/Scene/Transform.hpp"

#include "DemoCameraController.hpp"
#include "MeshRegistry.hpp"
#include "PopulateDemoScene.hpp"
#include "RenderDrawPlanning.hpp"

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
#include "vulkan/VulkanMesh.hpp"
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
    constexpr int kMaxFramesInFlight = 2;
    constexpr VkClearColorValue kClearColor{{0.06f, 0.30f, 0.42f, 1.0f}};

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
    Input::InputSystem inputSystem;

    Platform::WindowDesc desc;
    desc.title = "AREngine M12 Scene Render Demo";
    desc.width = 1280;
    desc.height = 720;
    auto window = Platform::CreateAppWindow(desc);

    bool framebufferResized = false;
    window->SetEventCallback([&framebufferResized, &inputSystem](Core::Event& event)
    {
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

    VulkanDevice device(physicalDevice.device, physicalDevice.queueFamilies, /*enableSwapchainExtension=*/true);

    auto swapchain = std::make_unique<VulkanSwapchain>(
        physicalDevice.device, device.Get(), surface.Get(), physicalDevice.queueFamilies,
        window->GetWidth(), window->GetHeight());
    AR_LOG_INFO(std::format("Swapchain: {}x{}, {} image(s)",
                             swapchain->GetExtent().width, swapchain->GetExtent().height, swapchain->GetImageCount()));

    const VkFormat depthFormat = FindSupportedDepthFormat(physicalDevice.device);

    VulkanRenderPass renderPass(device.Get(), swapchain->GetImageFormat(), depthFormat);
    VulkanDescriptorSetLayout descriptorSetLayout(device.Get());
    VulkanGraphicsPipeline pipeline(device.Get(), renderPass.Get(), descriptorSetLayout.Get());

    auto depthImage = std::make_unique<VulkanImage>(
        physicalDevice.device, device.Get(), swapchain->GetExtent().width, swapchain->GetExtent().height,
        depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT);
    auto framebuffers = std::make_unique<VulkanFramebuffers>(
        device.Get(), renderPass.Get(), swapchain->GetImageViews(), depthImage->GetView(), swapchain->GetExtent());

    VulkanCommandPool commandPool(device.Get(), physicalDevice.queueFamilies.graphicsFamily);

    // M12: TWO mesh resources (cube + floor quad), each uploaded exactly
    // once - the milestone's own required "at least two mesh types"
    // proof, plus mesh REUSE (multiple Scene entities reference the same
    // cube MeshId - see PopulateDemoScene.cpp).
    auto cubeMesh = CreateVulkanMesh(
        physicalDevice.device, device.Get(), commandPool.Get(), device.GetGraphicsQueue(), Rendering::CreateCubeMesh());
    auto floorMesh = CreateVulkanMesh(
        physicalDevice.device, device.Get(), commandPool.Get(), device.GetGraphicsQueue(), Rendering::CreateQuadMesh());
    AR_LOG_INFO("Uploaded 2 persistent meshes (cube, floor quad) - never re-uploaded per frame or per entity");

    const ARDemo::DemoMeshIds meshIds{Scene::MeshId{1}, Scene::MeshId{2}};
    ARDemo::MeshRegistry meshRegistry;
    meshRegistry.Register(meshIds.cube, cubeMesh.get());
    meshRegistry.Register(meshIds.floor, floorMesh.get());

    // M13: two distinctly-colored checkerboard textures - one material
    // resource per demo material, each uploaded exactly once. Red/blue
    // are arbitrary; the point is that they are visually distinguishable
    // so "same mesh, different material" is actually observable.
    constexpr std::uint32_t kTextureWidth = 64;
    constexpr std::uint32_t kTextureHeight = 64;
    constexpr std::uint32_t kTextureTileSize = 8;
    const std::vector<std::uint8_t> redCheckerPixels = GenerateCheckerboardRGBA8(
        kTextureWidth, kTextureHeight, kTextureTileSize, {200, 40, 40, 255}, {80, 10, 10, 255});
    const std::vector<std::uint8_t> blueCheckerPixels = GenerateCheckerboardRGBA8(
        kTextureWidth, kTextureHeight, kTextureTileSize, {40, 80, 200, 255}, {10, 20, 80, 255});
    auto redTexture = CreateTextureFromPixels(
        physicalDevice.device, device.Get(), commandPool.Get(), device.GetGraphicsQueue(),
        kTextureWidth, kTextureHeight, redCheckerPixels.data());
    auto blueTexture = CreateTextureFromPixels(
        physicalDevice.device, device.Get(), commandPool.Get(), device.GetGraphicsQueue(),
        kTextureWidth, kTextureHeight, blueCheckerPixels.data());
    VulkanSampler sampler(device.Get()); // one sampler, shared by every material - describes filtering only, not a specific image

    const ARDemo::DemoMaterialIds materialIds{Scene::MaterialId{1}, Scene::MaterialId{2}};
    VulkanDescriptorPool descriptorPool(device.Get(), /*maxSets=*/2);
    VkDescriptorSet redDescriptorSet = descriptorPool.Allocate(descriptorSetLayout.Get());
    WriteCombinedImageSamplerDescriptor(device.Get(), redDescriptorSet, redTexture->GetView(), sampler.Get());
    VkDescriptorSet blueDescriptorSet = descriptorPool.Allocate(descriptorSetLayout.Get());
    WriteCombinedImageSamplerDescriptor(device.Get(), blueDescriptorSet, blueTexture->GetView(), sampler.Get());

    ARDemo::MaterialRegistry materialRegistry;
    materialRegistry.Register(materialIds.redChecker, redDescriptorSet);
    materialRegistry.Register(materialIds.blueChecker, blueDescriptorSet);

    Scene::Camera camera;
    camera.nearZ = 0.1f;
    camera.farZ = 100.0f;

    // Scene content (floor + reference cube + cubeA + cubeB(child of
    // reference cube) + moveOffsetCube) is all at roughly Z in
    // [-2.5, -2.0] near the world origin - see PopulateDemoScene.cpp.
    // Camera starts a few meters back along +Z, facing -Z (identity
    // rotation), which looks straight at it.
    Scene::Transform cameraTransform;
    cameraTransform.position = Core::Math::Vec3(0.0f, 0.8f, 2.5f);
    ARDemo::DemoCameraController cameraController;

    AR_ASSERT_MSG(physicalDevice.properties.limits.maxPushConstantsSize >= sizeof(MvpPushConstants),
        "Device's maxPushConstantsSize is smaller than MvpPushConstants - should be spec-impossible (guaranteed >= 128 bytes)");

    // M12's whole point: real Scene data, not a hand-rolled object list.
    // PopulateDemoScene is the SAME function tests/xr_demo.cpp calls -
    // one scene-content definition, two presentation paths.
    Scene::Scene scene;
    const ARDemo::DemoSceneEntities sceneEntities = ARDemo::PopulateDemoScene(scene, meshIds, materialIds);
    AR_LOG_INFO("Scene: 5 entities (floor, referenceCube, cubeA, cubeB[child of referenceCube], moveOffsetCube), "
                "2 meshes, 2 materials");

    Platform::SteadyClock clock;
    double totalTimeSeconds = 0.0;

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

    std::vector<VkFence> imagesInFlight(swapchain->GetImageCount(), VK_NULL_HANDLE);

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

        framebuffers.reset();
        depthImage.reset();
        swapchain.reset();
        swapchain = std::make_unique<VulkanSwapchain>(
            physicalDevice.device, device.Get(), surface.Get(), physicalDevice.queueFamilies,
            window->GetWidth(), window->GetHeight());
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
    };

    AR_LOG_INFO("AREngine M12 scene render demo: WASD to move, Space/Ctrl for up/down, "
                "hold Right Mouse Button and move the mouse to look around. Close the window to exit.");

    int currentFrame = 0;
    std::uint32_t loggedFrameCount = 0;
    while (!window->ShouldClose())
    {
        inputSystem.BeginFrame();
        window->PollEvents();
        if (window->ShouldClose())
        {
            break;
        }

        const float deltaTimeSeconds = static_cast<float>(clock.Tick());
        totalTimeSeconds += deltaTimeSeconds;

        if (window->GetWidth() == 0 || window->GetHeight() == 0)
        {
            continue;
        }

        cameraController.Update(cameraTransform, inputSystem, deltaTimeSeconds);

        // Slow rotation on the reference cube, same animation
        // tests/xr_demo.cpp applies - proves a Scene entity's transform
        // can be mutated every frame with no stale extraction: cubeB
        // (its child) visibly swings through world space as a direct
        // consequence, with no code here aware cubeB even exists.
        scene.GetTransform(sceneEntities.referenceCube).rotation =
            Core::Math::Quaternion::FromAxisAngle(Core::Math::Vec3(0.0f, 1.0f, 0.0f), 0.5f * static_cast<float>(totalTimeSeconds));

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

        // M13: pipeline bound once per frame (shared by every material -
        // see docs/ARCHITECTURE.md, "M13 - Material & Render Resource
        // Binding Foundation"), but the descriptor set is NOT bound here
        // anymore - DrawPlannedInstances binds the correct material's
        // descriptor set per draw now that different renderables can use
        // different materials.
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.Get());

        // M12's whole data-flow proof: Scene -> ExtractRenderables ->
        // BuildDrawPlan (against this ONE desktop view) ->
        // DrawPlannedInstances. Extracted exactly once per frame, not
        // per anything else - there is only one view here, but the same
        // call shape works unchanged for xr_demo.cpp's N eye views.
        camera.SetAspectRatio(static_cast<float>(extent.width) / static_cast<float>(extent.height));
        const Core::Math::Mat4 viewProjection =
            ApplyVulkanYFlip(camera.GetProjectionMatrix()) * camera.GetViewMatrix(cameraTransform);
        const std::vector<Scene::RenderableInstance> renderables = scene.ExtractRenderables();
        const std::array<Core::Math::Mat4, 1> viewProjections{viewProjection};
        const std::vector<ARDemo::PlannedDraw> plan = ARDemo::BuildDrawPlan(renderables, viewProjections);
        ARDemo::DrawPlannedInstances(commandBuffer, pipeline.GetLayout(), meshRegistry, materialRegistry, plan);

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

        if (loggedFrameCount == 0)
        {
            AR_LOG_INFO(std::format("Frame {}: {} renderable(s) extracted, {} view(s), {} planned draw(s) "
                                     "(2 unique meshes, 2 unique materials, 2 texture uploads, 1 shared pipeline)",
                                     loggedFrameCount + 1, renderables.size(), viewProjections.size(), plan.size()));
        }
        ++loggedFrameCount;

        currentFrame = (currentFrame + 1) % kMaxFramesInFlight;
    }

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

    AR_LOG_INFO(std::format("Scene render demo complete - {} frame(s) rendered", loggedFrameCount));
    return 0;
}
