// Manual M9G validation demo — NOT part of the automated CTest suite,
// since it requires a real OpenXR loader/runtime with XR_KHR_vulkan_enable2
// support (and ideally a real or simulated HMD) that CI/headless systems
// may lack. Built by CMake but deliberately not registered with
// add_test. Run it manually.
//
// AREngine's first real 3D geometry rendered into OpenXR: a cube
// (M8H's exact reusable mesh/pipeline infrastructure, reused unchanged)
// drawn into each eye's real XR swapchain image, using the real per-
// view pose (inverted into a proper view matrix via M9G's new
// Core::Math::ViewMatrixFromPoseRH) and the real asymmetric projection
// M9F already produces from xrLocateViews. Bring-up (instance, system,
// view config, blend mode, swapchain format, graphics binding, session,
// LOCAL space, swapchains, XRFrameDriver) is identical in substance to
// tests/openxr_frame_demo.cpp - kept as a SEPARATE file rather than
// extending that one, specifically so neither file's concern (frame-
// lifecycle/view-location diagnostics vs. actual rendering) grows past
// a coherent, single-purpose size. See docs/ARCHITECTURE.md, "New Demo:
// openxr_cube_demo.cpp, Not Extending openxr_frame_demo.cpp (M9G)".
//
// Reaches directly into BOTH engine/xr/src/openxr/* AND
// engine/rendering/src/vulkan/* private implementation - the "M9G demo
// may coordinate these systems directly" the milestone brief grants,
// mirroring how vulkan_present_demo.cpp already reaches into
// Rendering's private headers and openxr_frame_demo.cpp already reaches
// into XR's. engine/xr itself gains NO new dependency on
// engine/rendering from this - only this leaf demo crosses both
// boundaries. No SceneRenderer, no Scene integration, no gameplay - one
// manually defined cube Transform is the entire "scene."
//
// One XrCompositionLayerProjection is still built via
// OpenXRProjectionLayer (M9F) - the only thing new here is that the
// swapchain images it references now actually contain rendered
// geometry instead of a flat color clear.

#include "AREngine/Core/Core.hpp"
#include "AREngine/Frame/Frame.hpp"
#include "AREngine/Rendering/ProceduralMesh.hpp"
#include "AREngine/Scene/Transform.hpp"

#include "openxr/OpenXREnvironmentBlendMode.hpp"
#include "openxr/OpenXRInstance.hpp"
#include "openxr/OpenXRProjectionLayer.hpp"
#include "openxr/OpenXRReferenceSpace.hpp"
#include "openxr/OpenXRResult.hpp"
#include "openxr/OpenXRSession.hpp"
#include "openxr/OpenXRSwapchain.hpp"
#include "openxr/OpenXRSystem.hpp"
#include "openxr/OpenXRVersion.hpp"
#include "openxr/OpenXRViewConfiguration.hpp"
#include "openxr/OpenXRVulkanGraphicsBinding.hpp"
#include "openxr/XRFrameDriver.hpp"

#include "vulkan/VulkanCheckerboard.hpp"
#include "vulkan/VulkanClipSpace.hpp"
#include "vulkan/VulkanCommandPool.hpp"
#include "vulkan/VulkanDepthFormat.hpp"
#include "vulkan/VulkanDescriptorPool.hpp"
#include "vulkan/VulkanDescriptorSetLayout.hpp"
#include "vulkan/VulkanFramebuffers.hpp"
#include "vulkan/VulkanGraphicsPipeline.hpp"
#include "vulkan/VulkanImage.hpp"
#include "vulkan/VulkanMesh.hpp"
#include "vulkan/VulkanPushConstants.hpp"
#include "vulkan/VulkanRenderPass.hpp"
#include "vulkan/VulkanSampler.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <numbers>
#include <vector>

namespace
{
    // Small, self-contained CheckVkResult-equivalent - same discipline
    // established throughout this codebase (duplicated per file rather
    // than shared across module boundaries).
    void CheckVkResultHere(VkResult result, const char* operation)
    {
        if (result != VK_SUCCESS)
        {
            const std::string message = std::format("{} failed: VkResult({})", operation, static_cast<int>(result));
            AR_LOG_ERROR(message);
            AR_ASSERT_MSG(false, message.c_str());
        }
    }

    std::string SwapchainFormatToString(std::int64_t format)
    {
        switch (static_cast<VkFormat>(format))
        {
            case VK_FORMAT_B8G8R8A8_SRGB: return "VK_FORMAT_B8G8R8A8_SRGB";
            case VK_FORMAT_R8G8B8A8_SRGB: return "VK_FORMAT_R8G8B8A8_SRGB";
            default:                      return std::format("VkFormat({})", format);
        }
    }

    // Background clear color, distinct per eye - purely diagnostic
    // (same philosophy as M9E's kEyeClearColors), now applied via the
    // render pass's own loadOp=CLEAR clear value instead of a separate
    // vkCmdClearColorImage.
    constexpr std::array<VkClearColorValue, 2> kEyeClearColors{
        VkClearColorValue{{0.05f, 0.05f, 0.12f, 1.0f}},
        VkClearColorValue{{0.12f, 0.05f, 0.05f, 1.0f}},
    };

    constexpr std::uint32_t kTargetFrameCount = 200;
    constexpr std::chrono::seconds kMaxDemoDuration{120};
}

int main()
{
    using namespace AREngine::XR::OpenXR;
    using namespace AREngine::Rendering::Vulkan;
    namespace Frame = AREngine::Frame;
    namespace Rendering = AREngine::Rendering;
    namespace Scene = AREngine::Scene;
    namespace Math = AREngine::Core::Math;

    AR_LOG_INFO(std::format("AREngine OpenXR cube demo - header version {}, requesting API version {}",
                             FormatXrVersion(XR_CURRENT_API_VERSION), FormatXrVersion(kTargetApiVersion)));

    // --- Bring-up: identical in substance to openxr_frame_demo.cpp -
    // see that file for the full reasoning behind each stop condition. ---
    const std::vector<XrExtensionProperties> extensions = EnumerateInstanceExtensions();
    if (!IsExtensionSupported(extensions, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME))
    {
        AR_LOG_WARNING(std::format("{} is NOT supported by the active OpenXR runtime - stopping here.",
                                    XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME));
        AR_LOG_INFO("OpenXR cube demo exiting cleanly (XR_KHR_vulkan_enable2 unavailable)");
        return 0;
    }

    const std::array<const char*, 1> requestedExtensions{XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
    OpenXRInstance instance(requestedExtensions);
    if (!instance.IsValid())
    {
        if (instance.CreationResult() == XR_ERROR_RUNTIME_UNAVAILABLE)
        {
            AR_LOG_WARNING("No active OpenXR runtime found (XR_ERROR_RUNTIME_UNAVAILABLE).");
        }
        else
        {
            AR_LOG_WARNING(std::format("xrCreateInstance failed unexpectedly: {}",
                                        XrResultToReadableString(XR_NULL_HANDLE, instance.CreationResult())));
        }
        AR_LOG_INFO("OpenXR cube demo exiting cleanly (no instance available)");
        return 0;
    }

    XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
    CheckXrResult(instance.Get(), xrGetInstanceProperties(instance.Get(), &instanceProperties), "xrGetInstanceProperties");
    AR_LOG_INFO(std::format("Active OpenXR runtime: {} (version {})",
                             instanceProperties.runtimeName, FormatXrVersion(instanceProperties.runtimeVersion)));

    const SystemRequestResult systemResult = TryGetHmdSystem(instance.Get());
    if (!systemResult.found)
    {
        if (IsFormFactorUnavailable(systemResult.rawResult))
        {
            AR_LOG_WARNING("OpenXR runtime is active, but no head-mounted-display system is currently available.");
        }
        else
        {
            AR_LOG_WARNING(std::format("xrGetSystem failed unexpectedly: {}",
                                        XrResultToReadableString(instance.Get(), systemResult.rawResult)));
        }
        AR_LOG_INFO("OpenXR cube demo exiting cleanly (runtime present, no HMD system)");
        return 0;
    }
    AR_LOG_INFO(std::format("HMD system acquired: XrSystemId {}", systemResult.systemId));
    const XrSystemId systemId = systemResult.systemId;

    const std::vector<XrViewConfigurationType> viewConfigTypes = EnumerateViewConfigurationTypes(instance.Get(), systemId);
    const std::optional<XrViewConfigurationType> primaryViewConfigType = SelectPrimaryViewConfigurationType(viewConfigTypes);
    if (!primaryViewConfigType.has_value())
    {
        AR_LOG_WARNING("XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO is NOT supported by this runtime/system - stopping here.");
        AR_LOG_INFO("OpenXR cube demo exiting cleanly (no stereo view configuration)");
        return 0;
    }
    AR_LOG_INFO("XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO is supported - selected as the primary view configuration");

    const std::vector<XrViewConfigurationView> viewConfigViews =
        EnumerateViewConfigurationViews(instance.Get(), systemId, *primaryViewConfigType);
    AR_LOG_INFO(std::format("View count: {}", viewConfigViews.size()));
    for (std::size_t i = 0; i < viewConfigViews.size(); ++i)
    {
        const XrViewConfigurationView& view = viewConfigViews[i];
        AR_LOG_INFO(std::format("  View {}: recommended {}x{} (max {}x{}), recommended sample count {} (max {})",
                                 i, view.recommendedImageRectWidth, view.recommendedImageRectHeight,
                                 view.maxImageRectWidth, view.maxImageRectHeight,
                                 view.recommendedSwapchainSampleCount, view.maxSwapchainSampleCount));
    }

    const std::vector<XrEnvironmentBlendMode> supportedBlendModes =
        EnumerateEnvironmentBlendModes(instance.Get(), systemId, *primaryViewConfigType);
    const std::optional<XrEnvironmentBlendMode> selectedBlendMode = SelectEnvironmentBlendMode(supportedBlendModes);
    if (!selectedBlendMode.has_value())
    {
        AR_LOG_WARNING("None of OPAQUE/ALPHA_BLEND/ADDITIVE is supported by this runtime - stopping here.");
        AR_LOG_INFO("OpenXR cube demo exiting cleanly (no usable environment blend mode)");
        return 0;
    }
    AR_LOG_INFO(std::format("Selected environment blend mode: {}", EnvironmentBlendModeToString(*selectedBlendMode)));

    AR_LOG_INFO("Creating OpenXR-compatible Vulkan instance/device via XR_KHR_vulkan_enable2...");
    OpenXRVulkanGraphicsBinding binding(instance.Get(), systemId);
    AR_LOG_INFO(std::format("Vulkan API version selected: {}", FormatVkApiVersion(binding.GetSelectedVulkanApiVersion())));
    const VulkanGraphicsBindingData& bindingData = binding.GetBindingData();

    OpenXRSession session(instance.Get(), systemId, bindingData);
    AR_LOG_INFO("XrSession created successfully");

    OpenXRReferenceSpace localSpace(instance.Get(), session.Get(), XR_REFERENCE_SPACE_TYPE_LOCAL);
    AR_LOG_INFO("Created LOCAL reference space");

    const std::vector<std::int64_t> supportedFormats = EnumerateSwapchainFormats(instance.Get(), session.Get());
    const std::optional<std::int64_t> selectedFormat = SelectSwapchainColorFormat(supportedFormats);
    if (!selectedFormat.has_value())
    {
        AR_LOG_WARNING("The runtime reported zero swapchain formats - stopping here.");
        AR_LOG_INFO("OpenXR cube demo exiting cleanly (no usable swapchain format)");
        return 0;
    }
    AR_LOG_INFO(std::format("Selected swapchain format: {}", SwapchainFormatToString(*selectedFormat)));

    // --- One XrSwapchain per view (M9E topology, unchanged), now
    // VkDevice-aware so each swapchain caches its own AREngine-owned
    // VkImageViews (M9G) - see OpenXRSwapchain.hpp. ---
    std::vector<std::unique_ptr<OpenXRSwapchain>> swapchains;
    swapchains.reserve(viewConfigViews.size());
    for (std::size_t i = 0; i < viewConfigViews.size(); ++i)
    {
        const XrViewConfigurationView& view = viewConfigViews[i];
        swapchains.push_back(std::make_unique<OpenXRSwapchain>(
            instance.Get(), session.Get(), bindingData.device, *selectedFormat,
            view.recommendedImageRectWidth, view.recommendedImageRectHeight, view.recommendedSwapchainSampleCount));
        AR_LOG_INFO(std::format("Created swapchain for view {}: {}x{}, {} image(s)/view(s)",
                                 i, swapchains.back()->GetWidth(), swapchains.back()->GetHeight(), swapchains.back()->GetImages().size()));
    }

    std::vector<XrSwapchainSubImage> projectionSubImages;
    projectionSubImages.reserve(swapchains.size());
    for (const std::unique_ptr<OpenXRSwapchain>& swapchain : swapchains)
    {
        XrSwapchainSubImage subImage{};
        subImage.swapchain = swapchain->Get();
        subImage.imageRect.offset = {0, 0};
        subImage.imageRect.extent = {static_cast<std::int32_t>(swapchain->GetWidth()), static_cast<std::int32_t>(swapchain->GetHeight())};
        subImage.imageArrayIndex = 0;
        projectionSubImages.push_back(subImage);
    }
    OpenXRProjectionLayer projectionLayer(projectionSubImages, localSpace.Get());

    // --- M9G: real Vulkan render infrastructure, reused from M8 almost
    // entirely unchanged (see docs/ARCHITECTURE.md, "Vulkan Reuse
    // Review (M9G)" for the full class-by-class report). ---
    const VkFormat depthFormat = FindSupportedDepthFormat(bindingData.physicalDevice);
    AR_LOG_INFO(std::format("Depth format: {}", static_cast<int>(depthFormat)));

    // One render pass/pipeline/descriptor-set-layout, shared across
    // both eyes - format-dependent only (color format + depth format),
    // not extent-dependent, so sharing is correct even if the two
    // eyes' extents ever differ (they don't today, but this was never
    // assumed - see the per-view depth/framebuffer loop below).
    // OpenXR-owned swapchain images are never presented via
    // vkQueuePresentKHR - VK_IMAGE_LAYOUT_PRESENT_SRC_KHR (this class's
    // desktop-oriented default) is meaningless for them and rejected by
    // validation. Leave them in COLOR_ATTACHMENT_OPTIMAL instead - see
    // docs/ARCHITECTURE.md, "VulkanRenderPass colorFinalLayout
    // Generalization (M9G)".
    VulkanRenderPass renderPass(bindingData.device, static_cast<VkFormat>(*selectedFormat), depthFormat,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VulkanDescriptorSetLayout descriptorSetLayout(bindingData.device);
    VulkanGraphicsPipeline pipeline(bindingData.device, renderPass.Get(), descriptorSetLayout.Get());

    VulkanCommandPool commandPool(bindingData.device, bindingData.queueFamilyIndex);

    // ONE cube mesh (Rendering::CreateCubeMesh - M8H's exact
    // backend-independent 1x1x1 cube), uploaded once, drawn twice per
    // frame (once per eye) with different push constants - never a
    // duplicate XR-specific mesh/vertex format.
    const Rendering::MeshData cubeMeshData = Rendering::CreateCubeMesh();
    auto cubeMesh = CreateVulkanMesh(bindingData.physicalDevice, bindingData.device, commandPool.Get(), binding.GetQueue(), cubeMeshData);
    AR_LOG_INFO(std::format("Cube mesh: {} vertices, {} indices, uploaded once", cubeMeshData.vertices.size(), cubeMeshData.indices.size()));

    // The existing M8E checkerboard texture path, reused unchanged - a
    // simple textured cube is enough for M9G, no PBR/lighting/material
    // system.
    constexpr std::uint32_t kTextureWidth = 64;
    constexpr std::uint32_t kTextureHeight = 64;
    constexpr std::uint32_t kTextureTileSize = 8;
    const std::vector<std::uint8_t> checkerboardPixels = GenerateCheckerboardRGBA8(kTextureWidth, kTextureHeight, kTextureTileSize);
    auto texture = CreateTextureFromPixels(
        bindingData.physicalDevice, bindingData.device, commandPool.Get(), binding.GetQueue(),
        kTextureWidth, kTextureHeight, checkerboardPixels.data());
    VulkanSampler sampler(bindingData.device);

    VulkanDescriptorPool descriptorPool(bindingData.device);
    VkDescriptorSet descriptorSet = descriptorPool.Allocate(descriptorSetLayout.Get());
    WriteCombinedImageSamplerDescriptor(bindingData.device, descriptorSet, texture->GetView(), sampler.Get());

    AR_ASSERT_MSG(binding.GetPhysicalDeviceProperties().limits.maxPushConstantsSize >= sizeof(MvpPushConstants),
        "Device's maxPushConstantsSize is smaller than MvpPushConstants - should be spec-impossible (guaranteed >= 128 bytes)");

    // --- Per-view depth image + framebuffers. TWO independent
    // VulkanFramebuffers instances (not one) - VulkanFramebuffers
    // itself is built around one shared extent/depth view per instance
    // (matching one desktop swapchain, whose images are always the
    // same size); two eyes are two independent OpenXRSwapchains that
    // could in principle have different extents, so each gets its own
    // depth image + framebuffers, sized to THAT view's own real
    // swapchain extent - never hard-coded, never assumed equal across
    // eyes even though both happen to be 1852x2056 on this runtime. ---
    std::vector<std::unique_ptr<VulkanImage>> depthImages;
    std::vector<std::unique_ptr<VulkanFramebuffers>> framebuffers;
    depthImages.reserve(swapchains.size());
    framebuffers.reserve(swapchains.size());
    for (const std::unique_ptr<OpenXRSwapchain>& swapchain : swapchains)
    {
        const VkExtent2D extent{swapchain->GetWidth(), swapchain->GetHeight()};
        depthImages.push_back(std::make_unique<VulkanImage>(
            bindingData.physicalDevice, bindingData.device, extent.width, extent.height,
            depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT));
        framebuffers.push_back(std::make_unique<VulkanFramebuffers>(
            bindingData.device, renderPass.Get(), swapchain->GetImageViews(), depthImages.back()->GetView(), extent));
    }

    // --- One cube, one world transform, shared across every view -
    // this is the invariant M9G exists to prove: N views of the SAME
    // model, not N cube instances. Stationary (identity rotation) -
    // "a stationary cube is preferred first because spatial stability
    // is easier to reason about" - optional slow rotation was
    // deliberately not added: SteamVR's null driver gives no way to
    // visually confirm motion either way in this environment, and M9F
    // already established this runtime's poses are static, so motion
    // would add complexity with no way to observe its effect here. See
    // docs/ARCHITECTURE.md, "Cube Placement (M9G)" for the pose-derived
    // visibility check that justifies these exact numbers (not a blind
    // guess). ---
    Scene::Transform cubeTransform;
    cubeTransform.position = Math::Vec3(0.0f, 0.0f, -2.0f);
    cubeTransform.scale = Math::Vec3(0.5f, 0.5f, 0.5f);
    AR_LOG_INFO(std::format("Cube: position=({:.2f},{:.2f},{:.2f}) scale={:.2f} (LOCAL space, stationary)",
                             cubeTransform.position.x, cubeTransform.position.y, cubeTransform.position.z, cubeTransform.scale.x));

    // --- Minimal Vulkan synchronization: one reusable command buffer,
    // one fence, fully synchronous - same "correctness first, prefer a
    // fence, document as temporary" pattern M9E already established
    // and documented (see docs/ARCHITECTURE.md, "Synchronization
    // Before Release (M9E)"), reused here unchanged for the same
    // reason, now covering real render-pass GPU work instead of a
    // flat clear. ---
    VkCommandBufferAllocateInfo cbAllocInfo{};
    cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbAllocInfo.commandPool = commandPool.Get();
    cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAllocInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    CheckVkResultHere(vkAllocateCommandBuffers(bindingData.device, &cbAllocInfo, &commandBuffer), "vkAllocateCommandBuffers");

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence renderFence = VK_NULL_HANDLE;
    CheckVkResultHere(vkCreateFence(bindingData.device, &fenceInfo, nullptr, &renderFence), "vkCreateFence");

    // --- Frame loop, driven through XRFrameDriver (M9E.5/M9F) ---
    AR_LOG_INFO(std::format("Beginning OpenXR cube render loop - target {} completed frames before requesting exit...", kTargetFrameCount));

    XRFrameDriver frameDriver(instance.Get(), session, localSpace, *primaryViewConfigType, *selectedBlendMode);

    std::uint32_t completedFrameCount = 0;
    std::uint32_t renderedFrameCount = 0;
    bool exitRequested = false;
    const auto startTime = std::chrono::steady_clock::now();

    while (true)
    {
        const Frame::FrameContext frameContext = frameDriver.PrepareFrame();

        if (frameContext.status == Frame::FrameStatus::Stop)
        {
            AR_LOG_INFO("Frame driver reports FrameStatus::Stop - stopping the main loop cleanly (not an error)");
            break;
        }

        if (frameContext.status == Frame::FrameStatus::Idle)
        {
            if (std::chrono::steady_clock::now() - startTime > kMaxDemoDuration)
            {
                AR_LOG_WARNING("Safety timeout reached while the frame driver was idle - stopping.");
                break;
            }
            continue;
        }

        frameDriver.BeginFrame();

        bool renderedThisFrame = false;

        if (frameContext.timing.shouldRender)
        {
            // Real view location - only when rendering is actually
            // requested (never call xrLocateViews unnecessarily).
            const std::vector<Frame::ViewInfo> views = frameDriver.GetViews();

            // Gate on OpenXRProjectionLayer::Prepare's own view-count-
            // vs-sub-image-count check (already logs a clear error and
            // fails safely on a mismatch, and views/rawViews are always
            // the same size by construction - see
            // docs/ARCHITECTURE.md) - no redundant second check.
            if (projectionLayer.Prepare(frameDriver.GetLastLocatedXrViews()) && views.size() == swapchains.size())
            {
                CheckVkResultHere(vkResetCommandBuffer(commandBuffer, 0), "vkResetCommandBuffer");
                VkCommandBufferBeginInfo cbBeginInfo{};
                cbBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                cbBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                CheckVkResultHere(vkBeginCommandBuffer(commandBuffer, &cbBeginInfo), "vkBeginCommandBuffer");

                std::vector<std::uint32_t> acquiredIndices(swapchains.size());
                for (std::size_t i = 0; i < swapchains.size(); ++i)
                {
                    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                    CheckXrResult(instance.Get(),
                        xrAcquireSwapchainImage(swapchains[i]->Get(), &acquireInfo, &acquiredIndices[i]), "xrAcquireSwapchainImage");

                    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                    waitInfo.timeout = XR_INFINITE_DURATION;
                    CheckXrResult(instance.Get(), xrWaitSwapchainImage(swapchains[i]->Get(), &waitInfo), "xrWaitSwapchainImage");
                }

                // Bound once, before the first eye's render pass - a
                // bound pipeline/vertex+index buffer/descriptor set
                // persists across vkCmdEndRenderPass -> vkCmdBeginRenderPass
                // within the same command buffer (Vulkan spec), so no
                // rebinding between eyes is needed.
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.Get());
                cubeMesh->Bind(commandBuffer);
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetLayout(),
                    0, 1, &descriptorSet, 0, nullptr);

                const Math::Mat4 model = cubeTransform.ToMatrix();

                for (std::size_t i = 0; i < views.size(); ++i)
                {
                    const VkExtent2D extent{swapchains[i]->GetWidth(), swapchains[i]->GetHeight()};

                    std::array<VkClearValue, 2> clearValues{};
                    clearValues[0].color = kEyeClearColors[i % kEyeClearColors.size()];
                    clearValues[1].depthStencil = {1.0f, 0};

                    VkRenderPassBeginInfo renderPassBegin{};
                    renderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                    renderPassBegin.renderPass = renderPass.Get();
                    renderPassBegin.framebuffer = framebuffers[i]->Get(acquiredIndices[i]);
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

                    // The real per-view pieces: viewFromWorld derived
                    // from this view's own located pose (M9G's new
                    // ViewMatrixFromPoseRH - NOT worldFromView used
                    // directly), and the real asymmetric projection
                    // xrLocateViews produced (M9F), Vulkan-Y-flipped via
                    // the SAME ApplyVulkanYFlip the desktop path uses
                    // (now fixed to correctly negate the whole row, not
                    // just m11 - see docs/ARCHITECTURE.md). Never
                    // Scene::Camera, never a symmetric/60-degree
                    // fallback - the runtime's own FOV is authoritative.
                    const Math::Mat4 view = Math::ViewMatrixFromPoseRH(views[i].position, views[i].orientation);
                    const Math::Mat4 projection = ApplyVulkanYFlip(views[i].projection);
                    const MvpPushConstants pushConstants{projection * view * model, Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f)};
                    vkCmdPushConstants(commandBuffer, pipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof(MvpPushConstants), &pushConstants);
                    cubeMesh->Draw(commandBuffer);

                    vkCmdEndRenderPass(commandBuffer);
                }

                CheckVkResultHere(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");

                CheckVkResultHere(vkResetFences(bindingData.device, 1, &renderFence), "vkResetFences");
                VkSubmitInfo submitInfo{};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &commandBuffer;
                CheckVkResultHere(vkQueueSubmit(binding.GetQueue(), 1, &submitInfo, renderFence), "vkQueueSubmit");

                // Wait for the GPU to finish before releasing the
                // swapchain images - xrReleaseSwapchainImage must never
                // be called while GPU work targeting that image is
                // still in flight. Same fence-based, fully-synchronous,
                // deliberately temporary strategy M9E already
                // documented.
                CheckVkResultHere(
                    vkWaitForFences(bindingData.device, 1, &renderFence, VK_TRUE, std::numeric_limits<std::uint64_t>::max()),
                    "vkWaitForFences");

                for (const std::unique_ptr<OpenXRSwapchain>& swapchain : swapchains)
                {
                    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                    CheckXrResult(instance.Get(), xrReleaseSwapchainImage(swapchain->Get(), &releaseInfo), "xrReleaseSwapchainImage");
                }

                frameDriver.SetPendingProjectionLayer(projectionLayer.Get());
                renderedThisFrame = true;
                ++renderedFrameCount;
            }

            const bool logSample = (completedFrameCount + 1 == 1) || ((completedFrameCount + 1) % 50 == 0);
            if (logSample)
            {
                if (renderedThisFrame)
                {
                    for (std::size_t i = 0; i < views.size(); ++i)
                    {
                        AR_LOG_INFO(std::format(
                            "  View {}: pos=({:.4f},{:.4f},{:.4f}) -> rendered into swapchain[{}] ({}x{})",
                            i, views[i].position.x, views[i].position.y, views[i].position.z,
                            i, swapchains[i]->GetWidth(), swapchains[i]->GetHeight()));
                    }
                }
                else
                {
                    AR_LOG_INFO("  Not rendered this frame (no valid views, or view/swapchain count mismatch)");
                }
            }
        }

        // EndFrame() always runs - submits the projection layer set
        // above if rendering happened this tick, zero layers otherwise
        // (shouldRender was false, or no valid views were located).
        frameDriver.EndFrame();

        ++completedFrameCount;
        if (completedFrameCount == 1 || completedFrameCount % 50 == 0)
        {
            AR_LOG_INFO(std::format("Completed frame {} (rendered={}, deltaTime={:.4f}s)",
                                     completedFrameCount, renderedThisFrame, frameContext.timing.deltaTimeSeconds));
        }

        if (completedFrameCount >= kTargetFrameCount && !exitRequested)
        {
            AR_LOG_INFO(std::format("Reached target frame count ({}) - requesting a clean session exit...", kTargetFrameCount));
            frameDriver.RequestExit();
            exitRequested = true;
        }

        if (std::chrono::steady_clock::now() - startTime > kMaxDemoDuration)
        {
            AR_LOG_WARNING("Safety timeout reached without the runtime reaching EXITING - stopping the demo loop anyway.");
            break;
        }
    }

    AR_LOG_INFO(std::format("Total completed frames: {}, rendered frames: {}, draw calls per rendered frame: {}",
                             completedFrameCount, renderedFrameCount, swapchains.size()));

    // AREngine's own submitted GPU work is already known complete (the
    // fence was waited on after every render), but the shared VkDevice
    // (handed to the runtime via XR_KHR_vulkan_enable2) may still have
    // SteamVR's own in-flight compositor work on it (its own command
    // buffers/fences/BlankEyeBuffer memory - same category already
    // documented as SteamVR-internal, not AREngine-attributable, in
    // M9E/M9E.5/M9F). Waiting for full device idle before tearing down
    // any Vulkan resource - the same discipline
    // tests/vulkan_present_demo.cpp already uses at its own shutdown -
    // avoids destroying anything the runtime might still be using.
    CheckVkResultHere(vkDeviceWaitIdle(bindingData.device), "vkDeviceWaitIdle");

    // Explicit cleanup of the raw Vulkan handles this demo created
    // directly, in dependency order, BEFORE returning - must happen
    // before `binding`'s destructor destroys the VkDevice they belong
    // to.
    vkDestroyFence(bindingData.device, renderFence, nullptr);
    // commandBuffer is freed implicitly when commandPool is destroyed
    // (its own destructor, below, in reverse declaration order).

    AR_LOG_INFO("OpenXR cube demo complete - shutting down");
    return 0;

    // Destruction, in exact reverse declaration order: frameDriver
    // (trivial - owns nothing) -> framebuffers -> depthImages ->
    // descriptorPool -> sampler -> texture -> cubeMesh -> commandPool
    // (frees commandBuffer implicitly) -> pipeline -> descriptorSetLayout
    // -> renderPass -> projectionLayer (trivial) -> swapchains (each
    // xrDestroySwapchain, and now each also destroys its own
    // AREngine-owned VkImageViews first) -> localSpace (xrDestroySpace)
    // -> session (xrDestroySession) -> binding (VkDevice, then
    // VkInstance) -> instance (xrDestroyInstance) last. Every GPU
    // resource this demo owns (framebuffers, depth images, pipeline,
    // render pass, mesh, texture) is destroyed while the VkDevice that
    // created it (bindingData.device, owned by `binding`) is still
    // alive, since `binding` is declared before all of them - this
    // ordering is a direct consequence of declaration order, not left
    // implicit or accidental (every dependency above is satisfied by
    // C++'s own reverse-local-destruction rule, verified against the
    // actual declaration order in this function, not assumed).
}
