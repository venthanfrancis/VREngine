// Manual M18 validation demo — NOT part of the automated CTest suite,
// since it requires a real Vulkan-capable GPU/driver and opens a real
// window, neither of which CI/headless systems have. Built by CMake but
// deliberately not registered with add_test. Run it manually. Gated
// behind ARENGINE_ENABLE_PHYSICS (as well as ARENGINE_ENABLE_VULKAN) -
// see tests/CMakeLists.txt.
//
// AREngine's first demo with real physical simulation: a static floor
// plus several dynamic bodies (a mix of the already-committed
// pyramid.obj asset [M15] and the procedural cube [M8H]) fall under
// gravity and settle, proving PhysicsWorld/PhysicsSceneBridge plug
// cleanly into the existing Scene -> ExtractRenderables ->
// BuildRenderItems -> SubmitRenderItems path (M12-M17) unchanged - this
// demo's only genuinely new code is the fixed-step physics loop and its
// own distinct scene content; presentation/bring-up is otherwise
// modeled closely on scene_render_demo.cpp (self-contained, no shared
// bring-up helper - matching this codebase's established per-demo
// convention). See docs/ARCHITECTURE.md, "M18 - Physics Foundation".
//
// Desktop-only: no XR integration this milestone (see
// docs/ARCHITECTURE.md for the documented Idle-frame/predictedDisplayTime
// policy a future XR integration would need to follow).

#include "AREngine/Assets/Assets.hpp"
#include "AREngine/Core/Core.hpp"
#include "AREngine/Input/Input.hpp"
#include "AREngine/Physics/FixedTimestepAccumulator.hpp"
#include "AREngine/Physics/PhysicsSceneBridge.hpp"
#include "AREngine/Physics/PhysicsWorld.hpp"
#include "AREngine/Platform/Platform.hpp"
#include "AREngine/Rendering/ProceduralMesh.hpp"
#include "AREngine/Scene/Camera.hpp"
#include "AREngine/Scene/Scene.hpp"
#include "AREngine/Scene/Transform.hpp"

#include "BuildRenderItems.hpp"
#include "DemoCameraController.hpp"
#include "PopulateDemoMaterials.hpp"
#include "PopulateDemoMeshes.hpp"
#include "PopulateDemoScene.hpp"

#include "vulkan/VulkanClipSpace.hpp"
#include "vulkan/VulkanCommandPool.hpp"
#include "vulkan/VulkanDepthFormat.hpp"
#include "vulkan/VulkanDescriptorSetLayout.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanFramebuffers.hpp"
#include "vulkan/VulkanGraphicsPipeline.hpp"
#include "vulkan/VulkanImage.hpp"
#include "vulkan/VulkanInstance.hpp"
#include "vulkan/VulkanPhysicalDevice.hpp"
#include "vulkan/VulkanPushConstants.hpp"
#include "vulkan/VulkanQueueFamilies.hpp"
#include "vulkan/VulkanRenderItemSubmission.hpp"
#include "vulkan/VulkanRenderPass.hpp"
#include "vulkan/VulkanRenderResourceContext.hpp"
#include "vulkan/VulkanResult.hpp"
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
    constexpr float kFixedDeltaSeconds = 1.0f / 60.0f;

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

    // One dynamic falling body's demo-authored setup - deliberately
    // simple/flat, not a "spawn config" framework. Box colliders are
    // authored to match each mesh's already-known bounds (both the
    // procedural cube and the pyramid.obj fixture span -0.5..+0.5 on
    // every axis - see docs/ARCHITECTURE.md, M15) - graphics and physics
    // geometry are independent in general (see docs/ARCHITECTURE.md,
    // "Mesh Size vs Collider Size"), they simply happen to coincide here
    // since both were deliberately authored to the same unit bounds.
    struct FallingBodySpawn
    {
        const char* name;
        Core::Math::Vec3 startPosition;
        Scene::MeshId mesh;
        Scene::MaterialId material;
    };
}

int main()
{
    Input::InputSystem inputSystem;

    Platform::WindowDesc desc;
    desc.title = "AREngine M18 Physics Demo";
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

    // Render resources - identical M14-M17 path every other demo uses.
    Assets::AssetManager assetManager(std::filesystem::path(AR_DEMO_ASSETS_ROOT));
    VulkanRenderResourceContext context(
        physicalDevice.device, device.Get(), commandPool.Get(), device.GetGraphicsQueue(),
        descriptorSetLayout.Get(), /*maxMaterials=*/2);

    const Scene::MeshId pyramidMeshId = ARDemo::PopulateDemoMeshes(assetManager, context);
    const Scene::MeshId cubeMeshId{context.CreateProceduralMesh(Rendering::CreateCubeMesh()).id};
    const Scene::MeshId floorMeshId{context.CreateProceduralMesh(Rendering::CreateQuadMesh()).id};
    AR_LOG_INFO(std::format("Uploaded {} GPU mesh resource(s) (1 asset-backed [pyramid.obj] + 2 procedural "
                             "[cube, floor quad])", context.MeshCount()));

    const ARDemo::DemoMaterialIds materialIds = ARDemo::PopulateDemoMaterials(assetManager, context);
    AR_LOG_INFO(std::format("Created {} GPU texture resource(s), {} material(s)",
                             context.TextureCount(), context.MaterialCount()));

    // --- M18: Physics world + Scene bridge. ---
    Physics::PhysicsWorld physicsWorld; // default gravity (0,-9.81,0), auto worker thread count
    Physics::PhysicsSceneBridge bridge;
    Physics::FixedTimestepAccumulator accumulator(kFixedDeltaSeconds);

    Scene::Scene scene;

    // Floor: static box, effective WORLD half-extents (5, 0.5, 5), top
    // surface at world Y=0. Rendered via the existing procedural quad
    // (faces local +Z by default - see ProceduralMesh.hpp), laid flat
    // with the same -90 degree X rotation PopulateDemoScene.cpp already
    // established for its own floor (quad's local +Z becomes world +Y).
    //
    // PhysicsSceneBridge::RegisterBody creates the physics body using
    // this SAME entity rotation (there is only one Transform per
    // entity, shared by rendering and physics) - so the BoxCollider's
    // half-extents below are given in the entity's LOCAL frame, not
    // world space. This -90-degree-about-X rotation maps local (x,y,z)
    // to world (x,z,-y), i.e. it swaps the Y and Z axes - so a collider
    // authored as locally (wide-X, wide-Z-becomes-Y, thin-Y-becomes-Z)
    // must swap the intended world Y/Z half-extents (0.5, 5.0) to local
    // (5.0, 0.5) to end up flat in world space. Passing the world-
    // intended (5, 0.5, 5) directly here would instead produce a body
    // that is thin in world Z and tall in world Y - a wall standing on
    // its edge, not a floor (caught by manual visual validation - see
    // docs/ARCHITECTURE.md, "M18 - Physics Foundation", "Collider Axes
    // Follow Body Rotation, Not Just Mesh Rotation").
    const Scene::EntityId floorEntity = scene.CreateEntity("Floor");
    scene.GetTransform(floorEntity).position = Core::Math::Vec3(0.0f, -0.5f, -3.0f);
    scene.GetTransform(floorEntity).rotation =
        Core::Math::Quaternion::FromAxisAngle(Core::Math::Vec3(1.0f, 0.0f, 0.0f), -1.5707963f);
    scene.GetTransform(floorEntity).scale = Core::Math::Vec3(10.0f, 10.0f, 1.0f); // scales the 1x1 quad to 10x10
    scene.SetRenderable(floorEntity, Scene::Renderable{floorMeshId, materialIds.redChecker, Core::Math::Vec4(0.6f, 0.6f, 0.6f, 1.0f), true});
    bridge.RegisterBody(scene, floorEntity, physicsWorld, Physics::BodyType::Static,
        Physics::BoxCollider{Core::Math::Vec3(5.0f, 5.0f, 0.5f)});

    // Dynamic falling bodies: a mix of asset-backed pyramids and
    // procedural cubes, different starting heights/positions, both
    // materials - proving M13-M16's existing "same mesh/different
    // material," "different mesh/same material," and "file-backed +
    // procedural geometry coexist" proofs still hold with physics
    // driving their transforms instead of hand-authored animation.
    const std::array<FallingBodySpawn, 4> spawns{{
        {"FallingPyramidA", Core::Math::Vec3(-1.5f, 3.0f, -3.0f), pyramidMeshId, materialIds.redChecker},
        {"FallingCubeA",    Core::Math::Vec3(-0.5f, 5.0f, -3.2f), cubeMeshId,    materialIds.blueChecker},
        {"FallingPyramidB", Core::Math::Vec3( 0.5f, 4.0f, -2.8f), pyramidMeshId, materialIds.blueChecker},
        {"FallingCubeB",    Core::Math::Vec3( 1.5f, 6.0f, -3.0f), cubeMeshId,    materialIds.redChecker},
    }};
    std::vector<Scene::EntityId> fallingEntities;
    fallingEntities.reserve(spawns.size());
    for (const FallingBodySpawn& spawn : spawns)
    {
        const Scene::EntityId entity = scene.CreateEntity(spawn.name);
        scene.GetTransform(entity).position = spawn.startPosition;
        scene.SetRenderable(entity, Scene::Renderable{spawn.mesh, spawn.material, Core::Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f), true});
        bridge.RegisterBody(scene, entity, physicsWorld, Physics::BodyType::Dynamic,
            Physics::BoxCollider{Core::Math::Vec3(0.5f, 0.5f, 0.5f)}, /*mass=*/1.0f);
        fallingEntities.push_back(entity);
    }
    AR_LOG_INFO(std::format("Scene: 1 static floor + {} dynamic bodies (2 asset-backed pyramids, 2 procedural cubes), "
                             "fixed step {:.4f}s, gravity (0,-9.81,0)", spawns.size(), kFixedDeltaSeconds));

    Scene::Camera camera;
    camera.nearZ = 0.1f;
    camera.farZ = 100.0f;

    Scene::Transform cameraTransform;
    cameraTransform.position = Core::Math::Vec3(0.0f, 2.0f, 3.0f);
    cameraTransform.rotation = Core::Math::Quaternion::FromAxisAngle(Core::Math::Vec3(1.0f, 0.0f, 0.0f), -0.15f);
    ARDemo::DemoCameraController cameraController;

    AR_ASSERT_MSG(physicalDevice.properties.limits.maxPushConstantsSize >= sizeof(MvpPushConstants),
        "Device's maxPushConstantsSize is smaller than MvpPushConstants - should be spec-impossible (guaranteed >= 128 bytes)");

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

    AR_LOG_INFO("AREngine M18 physics demo: WASD to move, Space/Ctrl for up/down, "
                "hold Right Mouse Button and move the mouse to look around. Close the window to exit.");

    int currentFrame = 0;
    std::uint32_t loggedFrameCount = 0;
    std::uint32_t totalPhysicsSteps = 0;
    bool loggedSettled = false;
    while (!window->ShouldClose())
    {
        inputSystem.BeginFrame();
        window->PollEvents();
        if (window->ShouldClose())
        {
            break;
        }

        const float deltaTimeSeconds = static_cast<float>(clock.Tick());

        if (window->GetWidth() == 0 || window->GetHeight() == 0)
        {
            continue;
        }

        cameraController.Update(cameraTransform, inputSystem, deltaTimeSeconds);

        // --- M18: fixed-step physics, decoupled from render framerate.
        // Never uses a synthetic delta and never steps once per view -
        // exactly one Step() per fixed tick, however many rendered
        // frames or eyes eventually consume the resulting Scene state.
        // See docs/ARCHITECTURE.md, "M18 - Physics Foundation". ---
        const int stepsThisFrame = accumulator.Advance(deltaTimeSeconds);
        for (int i = 0; i < stepsThisFrame; ++i)
        {
            physicsWorld.Step(accumulator.FixedDeltaSeconds());
        }
        totalPhysicsSteps += static_cast<std::uint32_t>(stepsThisFrame);
        if (stepsThisFrame > 0)
        {
            bridge.SyncDynamicBodiesToScene(scene, physicsWorld);
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

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.Get());

        // Unchanged M12-M17 path: Scene -> ExtractRenderables ->
        // BuildRenderItems -> SubmitRenderItems. Physics only ever
        // touches Scene::Transform via the bridge above - this code has
        // no idea physics exists.
        camera.SetAspectRatio(static_cast<float>(extent.width) / static_cast<float>(extent.height));
        const Core::Math::Mat4 viewProjection =
            ApplyVulkanYFlip(camera.GetProjectionMatrix()) * camera.GetViewMatrix(cameraTransform);
        const std::vector<Scene::RenderableInstance> renderables = scene.ExtractRenderables();
        const std::vector<Rendering::RenderItem> renderItems = ARDemo::BuildRenderItems(renderables);
        SubmitRenderItems(commandBuffer, pipeline.GetLayout(), context, viewProjection, renderItems);

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
            AR_LOG_INFO(std::format("Frame {}: {} renderable(s) extracted, {} render item(s) built, 1 view, "
                                     "{} draw(s) expected",
                                     loggedFrameCount + 1, renderables.size(), renderItems.size(), renderItems.size()));
        }
        ++loggedFrameCount;

        // Cheap correctness signal distinct from "it looks plausible" -
        // once ~4 seconds of simulated time have run, every dynamic body
        // should have landed; log each one's settled Y/rotation once,
        // deliberately not every frame. No debug collider renderer is
        // built for M18 - this text diagnostic is the intentional
        // substitute (see docs/ARCHITECTURE.md).
        if (!loggedSettled && totalPhysicsSteps > 240)
        {
            loggedSettled = true;
            for (std::size_t i = 0; i < fallingEntities.size(); ++i)
            {
                const Scene::Transform& t = scene.GetTransform(fallingEntities[i]);
                AR_LOG_INFO(std::format("  {} settled at position ({:.3f}, {:.3f}, {:.3f}), rotation (w={:.3f}, x={:.3f}, y={:.3f}, z={:.3f})",
                                         spawns[i].name, t.position.x, t.position.y, t.position.z,
                                         t.rotation.w, t.rotation.x, t.rotation.y, t.rotation.z));
            }
        }

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

    AR_LOG_INFO(std::format("Physics demo complete - {} frame(s) rendered, {} physics step(s) simulated",
                             loggedFrameCount, totalPhysicsSteps));
    return 0;
}
