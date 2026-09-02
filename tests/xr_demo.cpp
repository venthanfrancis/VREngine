// Manual M10.5 validation demo — NOT part of the automated CTest suite,
// since it requires a real OpenXR loader/runtime with XR_KHR_vulkan_enable2
// support (and ideally a real or simulated HMD) that CI/headless systems
// may lack. Built by CMake but deliberately not registered with
// add_test. Run it manually.
//
// AREngine's first INTEGRATED XR demo - one process that exercises every
// major XR subsystem proven independently across M9A-M10 together, in
// the same OpenXR instance/session: bring-up (M9A/M9C/M9D) -> the
// generic XR frame lifecycle (M9E.5) -> real per-view pose/projection
// data (M9F) -> multiple objects rendered into each eye's real OpenXR
// swapchain image, using M9H's hardened per-view render-target model
// (M9G/M9H) -> the OpenXR action/input system, synced and queried every
// running frame alongside rendering (M10) -> a real
// XrCompositionLayerProjection submitted through the existing
// OpenXRProjectionLayer path (M9F). Nothing here is new OpenXR/Vulkan
// capability - every wrapper class this file uses (OpenXRSession,
// XRFrameDriver, OpenXRVulkanViewTarget, OpenXRActionSystem, ...) is
// reused completely unchanged from its own milestone. What M10.5 proves
// is that these systems coexist correctly in ONE coherent application
// loop - see docs/ARCHITECTURE.md, "M10.5 Implementation Notes" for the
// audit that justified this scope and the one deliberate extension it
// required (multiple objects/meshes per view - see
// OpenXRVulkanViewTarget.hpp's Begin/Draw/End split).
//
// Still no gameplay, no player, no weapons, no Scene integration (a
// real Scene evaluation - not a decision made in advance - concluded
// Scene provides no value for this demo's flat, non-hierarchical,
// 5-object set; see docs/ARCHITECTURE.md), no SceneRenderer, no hand
// tracking, no haptics. This is an ENGINE integration demo, not a game.
//
// Earlier bring-up demos (openxr_demo, openxr_vulkan_demo,
// openxr_session_demo, openxr_frame_demo, openxr_cube_demo,
// openxr_input_demo) are intentionally untouched and left in place as
// focused regression tools - this file sits alongside them, not in
// place of them.
//
// SteamVR's null driver is expected to expose shouldRender=true on only
// the first frame (M9E onward) and no real controller bound to any
// interaction profile (M10) in this environment - both are reported
// honestly at the end of the run, not worked around or fabricated.
//
// M10.6 adds the missing link M10/M10.5 left deliberately unbuilt:
// input still didn't affect anything. The right hand's queried generic
// Input::*ActionState values now drive a small XRInteractionState
// (tests/XRInteractionState.hpp - generic types only, no OpenXR/Vulkan
// dependency, fully unit-tested with synthetic values in
// tests/xr_interaction_tests.cpp) that this demo applies to the SAME
// shared scene objects every view already renders: select toggles the
// reference cube's tint, trigger scales it, move offsets one small
// cube, and aim-pose shows/hides/positions a small marker cube. With
// SteamVR's null driver reporting every action inactive, the honest,
// expected live result is a neutral, unchanged scene - see
// docs/ARCHITECTURE.md, "M10.6 Implementation Notes" for the full
// design and the explicit test-vs-live evidence split.

#include "AREngine/Core/Core.hpp"
#include "AREngine/Frame/Frame.hpp"
#include "AREngine/Input/ActionState.hpp"
#include "AREngine/Rendering/ProceduralMesh.hpp"
#include "AREngine/Scene/Scene.hpp"

#include "MeshRegistry.hpp"
#include "OpenXRVulkanViewTarget.hpp"
#include "PopulateDemoScene.hpp"
#include "RenderDrawPlanning.hpp"
#include "XRInteractionState.hpp"

#include "openxr/OpenXRActionSystem.hpp"
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
#include "vulkan/VulkanGraphicsPipeline.hpp"
#include "vulkan/VulkanMesh.hpp"
#include "vulkan/VulkanPushConstants.hpp"
#include "vulkan/VulkanRenderPass.hpp"
#include "vulkan/VulkanSampler.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

namespace
{
    // M11.3 diagnostic-only teardown tracer - temporary, to be removed
    // once the Meta clean-exit crash (docs/ARCHITECTURE.md, "Clean-Exit
    // Crash") is root-caused. Writes to std::cerr (unit-buffered per the
    // C++ standard - each `<<` flushes automatically - plus an explicit
    // .flush() here for certainty) rather than AR_LOG_INFO's std::cout,
    // which can lose buffered-but-unflushed output when the process is
    // killed by an unhandled exception; this is exactly the ambiguity
    // M11.2's crash evidence could not rule out. One instance placed
    // immediately after each real object's declaration - C++ destroys
    // local objects in exact reverse declaration order, so this
    // sentinel's destructor fires immediately BEFORE the real object's
    // own destructor runs, giving a precise "reached this point in
    // teardown" trace without touching any engine class's own
    // destructor.
    struct TeardownMarker
    {
        const char* label;
        explicit TeardownMarker(const char* l) : label(l) {}
        ~TeardownMarker()
        {
            std::cerr << "[TEARDOWN] about to destroy: " << label << std::endl;
            std::cerr.flush();
        }
    };

    // M11.3 diagnostic-only: names AREngine-created objects via
    // VK_EXT_debug_utils (already enabled on this instance whenever
    // validation is available - see OpenXRVulkanGraphicsBinding.cpp) so
    // any object a validation message reports can be matched directly
    // against AREngine's own known handles, rather than inferred. Silently
    // no-ops if the function isn't available (release builds, or
    // validation unavailable) - never fatal, this is diagnostics only.
    // Never used on runtime/OpenXR-owned handles (e.g. swapchain
    // VkImages) - only on objects this demo itself creates.
    void NameVulkanObject(VkInstance instance, VkDevice device, VkObjectType type, std::uint64_t handle, const char* name)
    {
        auto setName = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
            vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"));
        if (setName == nullptr)
        {
            return;
        }
        VkDebugUtilsObjectNameInfoEXT nameInfo{};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = type;
        nameInfo.objectHandle = handle;
        nameInfo.pObjectName = name;
        setName(device, &nameInfo);
    }

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

    constexpr std::array<VkClearColorValue, 2> kEyeClearColors{
        VkClearColorValue{{0.05f, 0.05f, 0.12f, 1.0f}},
        VkClearColorValue{{0.12f, 0.05f, 0.05f, 1.0f}},
    };

    // Raised to 1000, matching M9H's own "at least 1000 lifecycle
    // iterations" validation standard - this demo exercises both the
    // render path AND the input path together over the same run.
    constexpr std::uint32_t kTargetFrameCount = 1000;
    constexpr std::chrono::seconds kMaxDemoDuration{300};

    constexpr float kReferenceCubeRotationRadiansPerSecond = 0.5f;
    constexpr float kAnalogLogThreshold = 0.05f;

    // M12: scene content (floor, reference cube, cubeA/B, moveOffset
    // cube) is now real AREngine::Scene::Scene data, populated via the
    // SAME PopulateDemoScene function tests/scene_render_demo.cpp calls
    // - see docs/ARCHITECTURE.md, "M12 - Renderable Scene Integration
    // Foundation" (which also documents why this reverses M10.5/M10.6's
    // "Scene integration declined" evaluations: this milestone gives
    // Scene both a real hierarchy - cubeB is a child of referenceCube -
    // and a Renderable component, closing both gaps that evaluation
    // found). The pose marker alone stays outside Scene: its
    // transform/visibility are driven by live OpenXR aim-pose action
    // state recomputed every frame (ephemeral input visualization, not
    // authored world data), so it's still drawn directly via
    // DrawOpenXRViewObject below, exactly as before.

    // M10.6 tint/scale constants - purely a visualization choice
    // (distinguishing "highlighted" from "neutral" by eye), not a
    // gameplay balance decision.
    const AREngine::Core::Math::Vec4 kReferenceCubeBaseTint(1.0f, 1.0f, 1.0f, 1.0f);
    const AREngine::Core::Math::Vec4 kReferenceCubeHighlightTint(1.0f, 0.85f, 0.1f, 1.0f);
    const AREngine::Core::Math::Vec3 kReferenceCubeBaseScale(0.6f, 0.6f, 0.6f);
    const AREngine::Core::Math::Vec4 kPoseMarkerTint(0.2f, 0.85f, 1.0f, 1.0f);

    struct FrameDiagnostics
    {
        std::uint32_t framesAttempted = 0;
        std::uint32_t framesWithShouldRenderTrue = 0;
        std::uint32_t framesRendered = 0;
        std::uint32_t framesSynced = 0;
        std::uint64_t viewsRendered = 0;
        std::uint64_t objectsRendered = 0;
        std::uint64_t drawCalls = 0;
        double cpuPrepSecondsSum = 0.0;
        double vulkanSubmitSecondsSum = 0.0;
        std::uint32_t timedSampleCount = 0;
    };

    const char* HandName(AREngine::XR::OpenXR::Hand hand)
    {
        return hand == AREngine::XR::OpenXR::Hand::Left ? "Left" : "Right";
    }

    // Change-only input logging bookkeeping, per hand - same discipline
    // M10's openxr_input_demo.cpp already established, reused verbatim
    // in shape (not literally shared code - this demo is self-contained,
    // per this codebase's own established "no shared demo-setup helper"
    // convention).
    struct HandLogState
    {
        bool everLogged = false;
        bool selectActive = false;
        bool triggerActive = false;
        float triggerValue = 0.0f;
        bool moveActive = false;
        AREngine::Core::Math::Vec2 moveValue;
        bool poseActive = false;
        bool posePositionValid = false;
        bool poseOrientationValid = false;
    };

    void LogHandState(
        AREngine::XR::OpenXR::Hand hand, HandLogState& logState,
        const AREngine::Input::DigitalActionState& select,
        const AREngine::Input::AnalogActionState& trigger,
        const AREngine::Input::Vector2ActionState& move,
        const AREngine::Input::PoseActionState& pose)
    {
        const char* name = HandName(hand);

        if (!logState.everLogged)
        {
            AR_LOG_INFO(std::format(
                "  [{}] first observed state: select.active={} trigger.active={} move.active={} pose.active={}",
                name, select.active, trigger.active, move.active, pose.active));
            logState.everLogged = true;
        }

        if (select.pressed) AR_LOG_INFO(std::format("  [{}] Select: pressed", name));
        if (select.released) AR_LOG_INFO(std::format("  [{}] Select: released", name));
        if (select.active != logState.selectActive)
        {
            AR_LOG_INFO(std::format("  [{}] Select: active {} -> {}", name, logState.selectActive, select.active));
            logState.selectActive = select.active;
        }

        if (trigger.active != logState.triggerActive)
        {
            AR_LOG_INFO(std::format("  [{}] Trigger: active {} -> {}", name, logState.triggerActive, trigger.active));
            logState.triggerActive = trigger.active;
            logState.triggerValue = trigger.value;
        }
        else if (trigger.active && std::abs(trigger.value - logState.triggerValue) > kAnalogLogThreshold)
        {
            AR_LOG_INFO(std::format("  [{}] Trigger: {:.3f} -> {:.3f}", name, logState.triggerValue, trigger.value));
            logState.triggerValue = trigger.value;
        }

        if (move.active != logState.moveActive)
        {
            AR_LOG_INFO(std::format("  [{}] Move: active {} -> {}", name, logState.moveActive, move.active));
            logState.moveActive = move.active;
            logState.moveValue = move.value;
        }
        else if (move.active)
        {
            const float dx = move.value.x - logState.moveValue.x;
            const float dy = move.value.y - logState.moveValue.y;
            if (std::sqrt(dx * dx + dy * dy) > kAnalogLogThreshold)
            {
                AR_LOG_INFO(std::format("  [{}] Move: ({:.3f},{:.3f}) -> ({:.3f},{:.3f})",
                                         name, logState.moveValue.x, logState.moveValue.y, move.value.x, move.value.y));
                logState.moveValue = move.value;
            }
        }

        if (pose.active != logState.poseActive)
        {
            AR_LOG_INFO(std::format("  [{}] AimPose: active {} -> {}", name, logState.poseActive, pose.active));
            logState.poseActive = pose.active;
        }
        if (pose.positionValid != logState.posePositionValid)
        {
            AR_LOG_INFO(std::format("  [{}] AimPose: positionValid {} -> {}", name, logState.posePositionValid, pose.positionValid));
            logState.posePositionValid = pose.positionValid;
        }
        if (pose.orientationValid != logState.poseOrientationValid)
        {
            AR_LOG_INFO(std::format("  [{}] AimPose: orientationValid {} -> {}", name, logState.poseOrientationValid, pose.orientationValid));
            logState.poseOrientationValid = pose.orientationValid;
        }
    }
}

int main()
{
    using namespace AREngine::XR::OpenXR;
    using namespace AREngine::Rendering::Vulkan;
    namespace Frame = AREngine::Frame;
    namespace Rendering = AREngine::Rendering;
    namespace Scene = AREngine::Scene;
    namespace Math = AREngine::Core::Math;
    namespace Input = AREngine::Input;

    AR_LOG_INFO(std::format("AREngine integrated XR demo - header version {}, requesting API version {}",
                             FormatXrVersion(XR_CURRENT_API_VERSION), FormatXrVersion(kTargetApiVersion)));

    // --- Bring-up: identical in substance to every prior OpenXR demo in
    // this codebase (M9A-M10) - see openxr_cube_demo.cpp/openxr_input_demo.cpp
    // for the full reasoning behind each stop condition. Deliberately
    // NOT factored into a shared helper - see docs/ARCHITECTURE.md,
    // "Existing-Demo Audit (M10.5)" for why this remains demo-local,
    // consistent with every earlier demo's own established convention. ---
    const std::vector<XrExtensionProperties> extensions = EnumerateInstanceExtensions();
    if (!IsExtensionSupported(extensions, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME))
    {
        AR_LOG_WARNING(std::format("{} is NOT supported by the active OpenXR runtime - stopping here.",
                                    XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME));
        AR_LOG_INFO("Integrated XR demo exiting cleanly (XR_KHR_vulkan_enable2 unavailable)");
        return 0;
    }

    const std::array<const char*, 1> requestedExtensions{XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
    OpenXRInstance instance(requestedExtensions);
    const TeardownMarker teardownInstance("instance (xrDestroyInstance)");
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
        AR_LOG_INFO("Integrated XR demo exiting cleanly (no instance available)");
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
        AR_LOG_INFO("Integrated XR demo exiting cleanly (runtime present, no HMD system)");
        return 0;
    }
    AR_LOG_INFO(std::format("HMD system acquired: XrSystemId {}", systemResult.systemId));
    const XrSystemId systemId = systemResult.systemId;

    const std::vector<XrViewConfigurationType> viewConfigTypes = EnumerateViewConfigurationTypes(instance.Get(), systemId);
    const std::optional<XrViewConfigurationType> primaryViewConfigType = SelectPrimaryViewConfigurationType(viewConfigTypes);
    if (!primaryViewConfigType.has_value())
    {
        AR_LOG_WARNING("XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO is NOT supported by this runtime/system - stopping here.");
        AR_LOG_INFO("Integrated XR demo exiting cleanly (no stereo view configuration)");
        return 0;
    }

    const std::vector<XrViewConfigurationView> viewConfigViews =
        EnumerateViewConfigurationViews(instance.Get(), systemId, *primaryViewConfigType);
    AR_LOG_INFO(std::format("View count: {}", viewConfigViews.size()));

    const std::vector<XrEnvironmentBlendMode> supportedBlendModes =
        EnumerateEnvironmentBlendModes(instance.Get(), systemId, *primaryViewConfigType);
    const std::optional<XrEnvironmentBlendMode> selectedBlendMode = SelectEnvironmentBlendMode(supportedBlendModes);
    if (!selectedBlendMode.has_value())
    {
        AR_LOG_WARNING("None of OPAQUE/ALPHA_BLEND/ADDITIVE is supported by this runtime - stopping here.");
        AR_LOG_INFO("Integrated XR demo exiting cleanly (no usable environment blend mode)");
        return 0;
    }
    AR_LOG_INFO(std::format("Selected environment blend mode: {}", EnvironmentBlendModeToString(*selectedBlendMode)));

    AR_LOG_INFO("Creating OpenXR-compatible Vulkan instance/device via XR_KHR_vulkan_enable2...");
    OpenXRVulkanGraphicsBinding binding(instance.Get(), systemId);
    const TeardownMarker teardownBinding("binding (debug messenger, then vkDestroyDevice, then vkDestroyInstance)");
    const VulkanGraphicsBindingData& bindingData = binding.GetBindingData();
    AR_LOG_INFO(std::format("Vulkan API version selected: {}", FormatVkApiVersion(binding.GetSelectedVulkanApiVersion())));

    OpenXRSession session(instance.Get(), systemId, bindingData);
    const TeardownMarker teardownSession("session (xrDestroySession)");
    AR_LOG_INFO("XrSession created successfully");

    OpenXRReferenceSpace localSpace(instance.Get(), session.Get(), XR_REFERENCE_SPACE_TYPE_LOCAL);
    const TeardownMarker teardownLocalSpace("localSpace (xrDestroySpace, LOCAL)");
    AR_LOG_INFO("Created LOCAL reference space");

    // --- M10: action set + actions + bindings + attach + action spaces,
    // reused completely unchanged. ---
    OpenXRActionSystem actionSystem(instance.Get(), session.Get());
    const TeardownMarker teardownActionSystem("actionSystem (action spaces, actions, action set)");
    AR_LOG_INFO("Created 'gameplay' action set, suggested khr/simple_controller bindings, attached, created action spaces");

    // --- M9E: swapchain format + one XrSwapchain per view. ---
    const std::vector<std::int64_t> supportedFormats = EnumerateSwapchainFormats(instance.Get(), session.Get());
    const std::optional<std::int64_t> selectedFormat = SelectSwapchainColorFormat(supportedFormats);
    if (!selectedFormat.has_value())
    {
        AR_LOG_WARNING("The runtime reported zero swapchain formats - stopping here.");
        AR_LOG_INFO("Integrated XR demo exiting cleanly (no usable swapchain format)");
        return 0;
    }
    AR_LOG_INFO(std::format("Selected swapchain format: {}", SwapchainFormatToString(*selectedFormat)));

    std::vector<std::unique_ptr<OpenXRSwapchain>> swapchains;
    swapchains.reserve(viewConfigViews.size());
    for (const XrViewConfigurationView& view : viewConfigViews)
    {
        swapchains.push_back(std::make_unique<OpenXRSwapchain>(
            instance.Get(), session.Get(), bindingData.device, *selectedFormat,
            view.recommendedImageRectWidth, view.recommendedImageRectHeight, view.recommendedSwapchainSampleCount));
        AR_LOG_INFO(std::format("Created swapchain: {}x{}, {} image(s)/view(s)",
                                 swapchains.back()->GetWidth(), swapchains.back()->GetHeight(), swapchains.back()->GetImages().size()));
    }
    const TeardownMarker teardownSwapchains("swapchains (per-view: AREngine VkImageViews, then xrDestroySwapchain)");

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
    const TeardownMarker teardownProjectionLayer("projectionLayer (trivial, no owned handle)");

    // --- M9G/M9H: shared render pass/pipeline/descriptor-set-layout
    // (format-dependent only, safe to share across every view). ---
    const VkFormat depthFormat = FindSupportedDepthFormat(bindingData.physicalDevice);
    VulkanRenderPass renderPass(bindingData.device, static_cast<VkFormat>(*selectedFormat), depthFormat,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const TeardownMarker teardownRenderPass("renderPass (vkDestroyRenderPass)");
    VulkanDescriptorSetLayout descriptorSetLayout(bindingData.device);
    const TeardownMarker teardownDescriptorSetLayout("descriptorSetLayout (vkDestroyDescriptorSetLayout)");
    VulkanGraphicsPipeline pipeline(bindingData.device, renderPass.Get(), descriptorSetLayout.Get());
    const TeardownMarker teardownPipeline("pipeline (vkDestroyPipeline, vkDestroyPipelineLayout)");

    VulkanCommandPool commandPool(bindingData.device, bindingData.queueFamilyIndex);
    const TeardownMarker teardownCommandPool("commandPool (vkDestroyCommandPool, frees commandBuffer implicitly)");
    NameVulkanObject(bindingData.instance, bindingData.device, VK_OBJECT_TYPE_COMMAND_POOL,
        reinterpret_cast<std::uint64_t>(commandPool.Get()), "AREngine.xr_demo.commandPool");

    // --- M10.5: TWO persistent meshes - the cube (M8H) and a quad
    // (M8D/ProceduralMesh) used as the floor. Both uploaded exactly
    // once, before the frame loop - never per-frame, never per-object. ---
    auto cubeMesh = CreateVulkanMesh(bindingData.physicalDevice, bindingData.device, commandPool.Get(), binding.GetQueue(),
                                      Rendering::CreateCubeMesh());
    const TeardownMarker teardownCubeMesh("cubeMesh (vertex+index VulkanBuffer)");
    auto floorMesh = CreateVulkanMesh(bindingData.physicalDevice, bindingData.device, commandPool.Get(), binding.GetQueue(),
                                       Rendering::CreateQuadMesh());
    const TeardownMarker teardownFloorMesh("floorMesh (vertex+index VulkanBuffer)");
    AR_LOG_INFO("Uploaded 2 persistent meshes (cube, floor quad) - never re-uploaded per frame");

    // M13: two distinctly-colored checkerboard textures, one per demo
    // material - see docs/ARCHITECTURE.md, "M13 - Material & Render
    // Resource Binding Foundation" for the single-pipeline/multiple-
    // descriptor-set invariant this relies on.
    constexpr std::uint32_t kTextureWidth = 64;
    constexpr std::uint32_t kTextureHeight = 64;
    constexpr std::uint32_t kTextureTileSize = 8;
    const std::vector<std::uint8_t> redCheckerPixels = GenerateCheckerboardRGBA8(
        kTextureWidth, kTextureHeight, kTextureTileSize, {200, 40, 40, 255}, {80, 10, 10, 255});
    const std::vector<std::uint8_t> blueCheckerPixels = GenerateCheckerboardRGBA8(
        kTextureWidth, kTextureHeight, kTextureTileSize, {40, 80, 200, 255}, {10, 20, 80, 255});
    auto redTexture = CreateTextureFromPixels(
        bindingData.physicalDevice, bindingData.device, commandPool.Get(), binding.GetQueue(),
        kTextureWidth, kTextureHeight, redCheckerPixels.data());
    const TeardownMarker teardownRedTexture("redTexture (VkImageView, VkImage, VkDeviceMemory)");
    auto blueTexture = CreateTextureFromPixels(
        bindingData.physicalDevice, bindingData.device, commandPool.Get(), binding.GetQueue(),
        kTextureWidth, kTextureHeight, blueCheckerPixels.data());
    const TeardownMarker teardownBlueTexture("blueTexture (VkImageView, VkImage, VkDeviceMemory)");
    VulkanSampler sampler(bindingData.device); // one sampler, shared by every material
    const TeardownMarker teardownSampler("sampler (vkDestroySampler)");

    const ARDemo::DemoMaterialIds materialIds{Scene::MaterialId{1}, Scene::MaterialId{2}};
    VulkanDescriptorPool descriptorPool(bindingData.device, /*maxSets=*/2);
    const TeardownMarker teardownDescriptorPool("descriptorPool (vkDestroyDescriptorPool, frees both descriptorSets implicitly)");
    VkDescriptorSet redDescriptorSet = descriptorPool.Allocate(descriptorSetLayout.Get());
    WriteCombinedImageSamplerDescriptor(bindingData.device, redDescriptorSet, redTexture->GetView(), sampler.Get());
    VkDescriptorSet blueDescriptorSet = descriptorPool.Allocate(descriptorSetLayout.Get());
    WriteCombinedImageSamplerDescriptor(bindingData.device, blueDescriptorSet, blueTexture->GetView(), sampler.Get());

    ARDemo::MaterialRegistry materialRegistry;
    const TeardownMarker teardownMaterialRegistry("materialRegistry (trivial, no owned handle)");
    materialRegistry.Register(materialIds.redChecker, redDescriptorSet);
    materialRegistry.Register(materialIds.blueChecker, blueDescriptorSet);

    AR_ASSERT_MSG(binding.GetPhysicalDeviceProperties().limits.maxPushConstantsSize >= sizeof(MvpPushConstants),
        "Device's maxPushConstantsSize is smaller than MvpPushConstants - should be spec-impossible (guaranteed >= 128 bytes)");

    // --- M9H: one OpenXRVulkanViewTarget per view, built exactly once. ---
    std::vector<std::unique_ptr<OpenXRVulkanViewTarget>> viewTargets;
    viewTargets.reserve(swapchains.size());
    for (const std::unique_ptr<OpenXRSwapchain>& swapchain : swapchains)
    {
        viewTargets.push_back(std::make_unique<OpenXRVulkanViewTarget>(
            bindingData.physicalDevice, bindingData.device, renderPass.Get(), *swapchain, depthFormat));
    }
    const TeardownMarker teardownViewTargets("viewTargets (per-view: depth VulkanImage, then VulkanFramebuffers)");
    AR_LOG_INFO(std::format("Created {} per-view render target(s), built once", viewTargets.size()));

    // --- M12: scene content is real AREngine::Scene::Scene data - see
    // the comment above the (now-removed) SceneObject struct for the
    // full boundary/reasoning. ALL views render this SAME extracted
    // renderable list - never one set of objects per eye. ---
    const ARDemo::DemoMeshIds meshIds{Scene::MeshId{1}, Scene::MeshId{2}};
    ARDemo::MeshRegistry meshRegistry;
    meshRegistry.Register(meshIds.cube, cubeMesh.get());
    meshRegistry.Register(meshIds.floor, floorMesh.get());

    Scene::Scene scene;
    const ARDemo::DemoSceneEntities sceneEntities = ARDemo::PopulateDemoScene(scene, meshIds, materialIds);
    // Recorded once, right after PopulateDemoScene, rather than
    // duplicating its hard-coded initial position here - the single
    // source of truth for "where this cube starts" stays in
    // PopulateDemoScene.cpp.
    const Math::Vec3 moveOffsetCubeBasePosition = scene.GetTransform(sceneEntities.moveOffsetCube).position;

    AR_LOG_INFO("Scene: 5 entities (floor, referenceCube, cubeA, cubeB[child of referenceCube], moveOffsetCube), "
                "2 meshes, 2 materials, + 1 demo-owned pose marker (outside Scene - see the boundary note above), "
                "shared across every view");

    // --- M9E/M9G: one reusable command buffer, one fence, fully
    // synchronous - unchanged from every prior XR rendering demo. ---
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
    NameVulkanObject(bindingData.instance, bindingData.device, VK_OBJECT_TYPE_FENCE,
        reinterpret_cast<std::uint64_t>(renderFence), "AREngine.xr_demo.renderFence");

    // --- Frame loop, driven through XRFrameDriver. ---
    AR_LOG_INFO(std::format("Beginning integrated XR loop - target {} completed frames before requesting exit...", kTargetFrameCount));

    XRFrameDriver frameDriver(instance.Get(), session, localSpace, *primaryViewConfigType, *selectedBlendMode);
    const TeardownMarker teardownFrameDriver("frameDriver (trivial, no owned handle)");

    std::uint32_t completedFrameCount = 0;
    bool exitRequested = false;
    const auto startTime = std::chrono::steady_clock::now();
    FrameDiagnostics diag;
    std::array<HandLogState, 2> logStates{}; // [0]=Left, [1]=Right
    ARDemo::XRInteractionState interactionState; // M10.6 - one shared state, never per-view/per-eye

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

        // --- Input: synced every running (Continue) frame, independent
        // of shouldRender - xrSyncActions/xrGetActionState* are legal
        // and meaningful regardless of whether this tick renders. See
        // docs/ARCHITECTURE.md, "Input Update Order (M10.5)". ---
        actionSystem.SyncActions(instance.Get(), session.Get());
        ++diag.framesSynced;

        // M11.1B: report the runtime's ACTUAL reported interaction
        // profile per hand - never guessed, never hard-coded to a
        // specific runtime. Logged at frame 1 and then periodically
        // (same cadence as the existing frame-count log), since the
        // runtime may not bind a profile until later in the session
        // lifecycle (e.g. once FOCUSED is reached) - reporting only
        // once at frame 1 would risk a false "(none)" if it resolves
        // later. An empty string means XR_NULL_PATH (no profile bound
        // yet for that hand).
        if (completedFrameCount == 0 || (completedFrameCount + 1) % 100 == 0)
        {
            const std::string leftProfile = actionSystem.GetCurrentInteractionProfile(instance.Get(), session.Get(), Hand::Left);
            const std::string rightProfile = actionSystem.GetCurrentInteractionProfile(instance.Get(), session.Get(), Hand::Right);
            AR_LOG_INFO(std::format("  [frame {}] [Left] interaction profile: {}", completedFrameCount + 1,
                                     leftProfile.empty() ? "(none / XR_NULL_PATH)" : leftProfile));
            AR_LOG_INFO(std::format("  [frame {}] [Right] interaction profile: {}", completedFrameCount + 1,
                                     rightProfile.empty() ? "(none / XR_NULL_PATH)" : rightProfile));
        }

        const XrTime predictedDisplayTime = frameDriver.GetLastPredictedDisplayTime();
        for (const Hand hand : {Hand::Left, Hand::Right})
        {
            const Input::DigitalActionState select = actionSystem.GetSelectState(instance.Get(), session.Get(), hand);
            const Input::AnalogActionState trigger = actionSystem.GetTriggerState(instance.Get(), session.Get(), hand);
            const Input::Vector2ActionState move = actionSystem.GetMoveState(instance.Get(), session.Get(), hand);
            const Input::PoseActionState pose =
                actionSystem.GetAimPoseState(instance.Get(), session.Get(), localSpace.Get(), predictedDisplayTime, hand);
            LogHandState(hand, logStates[hand == Hand::Left ? 0 : 1], select, trigger, move, pose);

            // M10.6: the right hand drives the visible interaction
            // state - a simple, single-hand mapping (no evidence yet
            // justifies a two-hand combination policy). Computed every
            // running frame, independent of shouldRender - engine state
            // should not freeze just because the runtime skipped visual
            // rendering this tick. See docs/ARCHITECTURE.md, "Input
            // Update Order (M10.6)". These four calls only ever see
            // REAL queried OpenXR state, converted through M10's own
            // conversion layer - never synthetic/fabricated input; see
            // "No Fake Live Input (M10.6)".
            if (hand == Hand::Right)
            {
                ARDemo::ApplyDigitalToggle(select, interactionState);
                ARDemo::ApplyAnalogScale(trigger, interactionState);
                ARDemo::ApplyVectorOffset(move, interactionState);
                ARDemo::ApplyPoseMarker(pose, interactionState);
            }
        }

        bool renderedThisFrame = false;

        if (frameContext.timing.shouldRender)
        {
            ++diag.framesWithShouldRenderTrue;
            const auto prepStart = std::chrono::steady_clock::now();

            const std::vector<Frame::ViewInfo> views = frameDriver.GetViews();

            // M12: apply this frame's already-computed interactionState
            // directly to the Scene entities (no local SceneObject
            // shadow copy anymore), then extract + plan ONCE per frame -
            // independent of whether Prepare() below succeeds, since
            // neither step touches OpenXR/Vulkan state. This also fixes
            // a latent M10.5/M10.6 behavior: previously, a frame where
            // Prepare() failed left the reference cube's rotation/tint
            // and the move-offset cube's position silently un-updated
            // that frame; extracting unconditionally here means Scene
            // state always advances with real time/input, whether or
            // not that particular frame ends up rendering.
            {
                Scene::Transform& referenceCubeTransform = scene.GetTransform(sceneEntities.referenceCube);
                referenceCubeTransform.rotation = Math::Quaternion::FromAxisAngle(
                    Math::Vec3(0.0f, 1.0f, 0.0f),
                    kReferenceCubeRotationRadiansPerSecond * static_cast<float>(frameContext.timing.totalTimeSeconds));
                referenceCubeTransform.scale = kReferenceCubeBaseScale * interactionState.scaleFactor;
                // M13: this SetRenderable call fully replaces the
                // entity's Renderable every frame - materialIds.redChecker
                // must be passed explicitly here, matching referenceCube's
                // material as assigned in PopulateDemoScene, or this
                // object would silently stop rendering (its MaterialId
                // would default to invalid and resolve to VK_NULL_HANDLE,
                // which DrawPlannedInstances skips without error). Caught
                // during M13 design review - see docs/ARCHITECTURE.md,
                // "M13 - Material Field Placement".
                scene.SetRenderable(sceneEntities.referenceCube, Scene::Renderable{
                    meshIds.cube,
                    materialIds.redChecker,
                    interactionState.highlightEnabled ? kReferenceCubeHighlightTint : kReferenceCubeBaseTint,
                    true});

                scene.GetTransform(sceneEntities.moveOffsetCube).position = moveOffsetCubeBasePosition +
                    Math::Vec3(interactionState.moveOffset.x, interactionState.moveOffset.y, 0.0f);
            }

            const std::vector<Scene::RenderableInstance> renderables = scene.ExtractRenderables();
            std::vector<Math::Mat4> viewProjections;
            viewProjections.reserve(views.size());
            for (const Frame::ViewInfo& viewInfo : views)
            {
                viewProjections.push_back(
                    ApplyVulkanYFlip(viewInfo.projection) * Math::ViewMatrixFromPoseRH(viewInfo.position, viewInfo.orientation));
            }
            const std::vector<ARDemo::PlannedDraw> plan = ARDemo::BuildDrawPlan(renderables, viewProjections);

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

                // Pipeline + descriptor set bound once, before the first
                // view's render pass - shared across every view/object
                // this frame (Vulkan spec: bound state persists across
                // vkCmdEndRenderPass -> vkCmdBeginRenderPass within one
                // command buffer). Per-object mesh binding happens
                // inside DrawOpenXRViewObject instead - see
                // OpenXRVulkanViewTarget.hpp.
                // M13: pipeline bound once per frame (shared by every
                // material), but the descriptor set is NOT bound here
                // anymore - DrawPlannedInstances binds the correct
                // material's descriptor set per draw now that different
                // renderables can use different materials.
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.Get());

                // M12: per-view draw loop - scene renderables come from
                // the plan already built above (once per frame, sliced
                // per view here since BuildDrawPlan produces its output
                // grouped [view][renderable], contiguous per view - see
                // RenderDrawPlanning.hpp/tests). The pose marker (never
                // part of Scene/extraction - see the boundary note near
                // PopulateDemoScene's call above) is still drawn
                // directly via DrawOpenXRViewObject - M13: since the
                // pipeline-wide upfront descriptor-set bind is gone, the
                // pose marker now binds one of the two demo materials
                // explicitly right before its own draw (a demo-only
                // convenience reusing an existing material, not a
                // material system of its own for the pose marker).
                for (std::size_t i = 0; i < views.size(); ++i)
                {
                    BeginOpenXRViewRenderPass(commandBuffer, renderPass.Get(), *viewTargets[i], acquiredIndices[i],
                                               kEyeClearColors[i % kEyeClearColors.size()]);

                    const std::span<const ARDemo::PlannedDraw> viewPlan(
                        plan.data() + i * renderables.size(), renderables.size());
                    ARDemo::DrawPlannedInstances(commandBuffer, pipeline.GetLayout(), meshRegistry, materialRegistry, viewPlan);
                    diag.objectsRendered += viewPlan.size();
                    diag.drawCalls += viewPlan.size();

                    if (interactionState.poseMarkerVisible)
                    {
                        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetLayout(),
                            0, 1, &redDescriptorSet, 0, nullptr);
                        const Scene::Transform poseMarkerTransform{
                            interactionState.poseMarkerPosition, interactionState.poseMarkerOrientation,
                            Math::Vec3(0.15f, 0.15f, 0.15f)};
                        const Math::Mat4 poseMarkerMvp = viewProjections[i] * poseMarkerTransform.ToMatrix();
                        DrawOpenXRViewObject(commandBuffer, pipeline.GetLayout(), *cubeMesh, poseMarkerMvp, kPoseMarkerTint);
                        ++diag.objectsRendered;
                        ++diag.drawCalls;
                    }

                    EndOpenXRViewRenderPass(commandBuffer);
                }
                diag.viewsRendered += views.size();

                CheckVkResultHere(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");

                const auto submitStart = std::chrono::steady_clock::now();
                diag.cpuPrepSecondsSum += std::chrono::duration<double>(submitStart - prepStart).count();

                CheckVkResultHere(vkResetFences(bindingData.device, 1, &renderFence), "vkResetFences");
                VkSubmitInfo submitInfo{};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &commandBuffer;
                CheckVkResultHere(vkQueueSubmit(binding.GetQueue(), 1, &submitInfo, renderFence), "vkQueueSubmit");

                CheckVkResultHere(
                    vkWaitForFences(bindingData.device, 1, &renderFence, VK_TRUE, std::numeric_limits<std::uint64_t>::max()),
                    "vkWaitForFences");

                const auto submitEnd = std::chrono::steady_clock::now();
                diag.vulkanSubmitSecondsSum += std::chrono::duration<double>(submitEnd - submitStart).count();
                ++diag.timedSampleCount;

                for (const std::unique_ptr<OpenXRSwapchain>& swapchain : swapchains)
                {
                    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                    CheckXrResult(instance.Get(), xrReleaseSwapchainImage(swapchain->Get(), &releaseInfo), "xrReleaseSwapchainImage");
                }

                frameDriver.SetPendingProjectionLayer(projectionLayer.Get());
                renderedThisFrame = true;
                ++diag.framesRendered;
            }

            const bool logSample = (completedFrameCount + 1 == 1) || ((completedFrameCount + 1) % 100 == 0);
            if (logSample)
            {
                if (renderedThisFrame)
                {
                    const std::size_t visibleObjectCount = renderables.size() + (interactionState.poseMarkerVisible ? 1 : 0);
                    const std::size_t totalDrawCalls = plan.size() + (interactionState.poseMarkerVisible ? views.size() : 0);
                    AR_LOG_INFO(std::format("  Rendered {} view(s) x {} visible object(s) = {} draw(s) this frame "
                                             "(highlight={}, scale={:.2f}, moveOffset=({:.2f},{:.2f}), poseMarkerVisible={})",
                                             views.size(), visibleObjectCount, totalDrawCalls,
                                             interactionState.highlightEnabled, interactionState.scaleFactor,
                                             interactionState.moveOffset.x, interactionState.moveOffset.y,
                                             interactionState.poseMarkerVisible));
                }
                else
                {
                    AR_LOG_INFO("  Not rendered this frame (no valid views, or view/swapchain count mismatch)");
                }
            }
        }

        // EndFrame() always runs - submits the projection layer set
        // above if rendering happened this tick, zero layers otherwise
        // (shouldRender was false, or no valid views/count mismatch).
        frameDriver.EndFrame();

        ++completedFrameCount;
        ++diag.framesAttempted;
        if (completedFrameCount == 1 || completedFrameCount % 100 == 0)
        {
            AR_LOG_INFO(std::format("Completed frame {} (rendered={}, synced={}, deltaTime={:.4f}s)",
                                     completedFrameCount, renderedThisFrame, diag.framesSynced, frameContext.timing.deltaTimeSeconds));
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

    AR_LOG_INFO(std::format(
        "Diagnostics: frames attempted={}, frames synced={}, frames shouldRender=true={}, frames rendered={}, "
        "views rendered={}, objects rendered={}, draw calls={}",
        diag.framesAttempted, diag.framesSynced, diag.framesWithShouldRenderTrue, diag.framesRendered,
        diag.viewsRendered, diag.objectsRendered, diag.drawCalls));
    if (diag.timedSampleCount > 0)
    {
        AR_LOG_INFO(std::format(
            "Diagnostics: avg CPU frame-prep time={:.4f}ms, avg Vulkan submit-to-fence time={:.4f}ms (over {} rendered frame(s))",
            1000.0 * diag.cpuPrepSecondsSum / diag.timedSampleCount,
            1000.0 * diag.vulkanSubmitSecondsSum / diag.timedSampleCount,
            diag.timedSampleCount));
    }
    AR_LOG_INFO(std::format(
        "Final interaction state (right hand): highlightEnabled={}, scaleFactor={:.2f}, moveOffset=({:.2f},{:.2f}), poseMarkerVisible={}",
        interactionState.highlightEnabled, interactionState.scaleFactor,
        interactionState.moveOffset.x, interactionState.moveOffset.y, interactionState.poseMarkerVisible));
    if (!logStates[0].selectActive && !logStates[0].triggerActive && !logStates[0].moveActive && !logStates[0].poseActive &&
        !logStates[1].selectActive && !logStates[1].triggerActive && !logStates[1].moveActive && !logStates[1].poseActive)
    {
        AR_LOG_INFO("No action was ever active on either hand this run - consistent with this environment's SteamVR "
                    "null driver exposing no real controller bound to any interaction profile. Not fabricated. "
                    "The interaction state above is therefore the neutral/default state (highlight off, base scale, "
                    "zero move offset, pose marker hidden) - this proves the input-to-state wiring runs cleanly "
                    "against a real (inactive) runtime, NOT that a real controller drove a visible change - see "
                    "docs/ARCHITECTURE.md, \"Real Active-Input Limitation (M10.6)\" for that explicit distinction.");
    }

    // AREngine's own submitted GPU work is already known complete (the
    // fence was waited on after every render), but the shared VkDevice
    // may still have SteamVR's own in-flight compositor work on it -
    // same category already documented as SteamVR-internal since M9E.
    std::cerr << "[TEARDOWN] begin explicit shutdown sequence" << std::endl;
    std::cerr.flush();
    std::cerr << "[TEARDOWN] vkDeviceWaitIdle begin" << std::endl;
    std::cerr.flush();
    const VkResult waitIdleResult = vkDeviceWaitIdle(bindingData.device);
    std::cerr << "[TEARDOWN] vkDeviceWaitIdle returned " << static_cast<int>(waitIdleResult) << std::endl;
    std::cerr.flush();
    CheckVkResultHere(waitIdleResult, "vkDeviceWaitIdle");

    std::cerr << "[TEARDOWN] vkDestroyFence(renderFence) begin" << std::endl;
    std::cerr.flush();
    vkDestroyFence(bindingData.device, renderFence, nullptr);
    std::cerr << "[TEARDOWN] vkDestroyFence(renderFence) complete" << std::endl;
    std::cerr.flush();
    // commandBuffer is freed implicitly when commandPool is destroyed.

    AR_LOG_INFO("Integrated XR demo complete - shutting down");
    std::cerr << "[TEARDOWN] entering automatic (RAII) destructor chain now" << std::endl;
    std::cerr.flush();
    return 0;

    // Destruction, in exact reverse declaration order: frameDriver
    // (trivial) -> viewTargets (each destroys its own depth image +
    // framebuffers) -> moveOffsetCubeBasePosition/sceneEntities/scene/
    // materialIds/materialRegistry/meshRegistry/meshIds (all trivial -
    // Scene::Scene owns no GPU/OpenXR handle, MeshRegistry/MaterialRegistry
    // hold only raw non-owning pointers/handles) -> descriptorPool
    // (frees both redDescriptorSet/blueDescriptorSet implicitly) ->
    // sampler -> blueTexture -> redTexture ->
    // floorMesh -> cubeMesh -> commandPool (frees commandBuffer
    // implicitly) -> pipeline -> descriptorSetLayout -> renderPass ->
    // projectionLayer (trivial) -> swapchains (each xrDestroySwapchain,
    // and its own cached AREngine-owned VkImageViews first) ->
    // actionSystem (destroys both aim_pose action spaces, then all four
    // actions, then the action set itself - all while `session` is
    // still alive) -> localSpace (xrDestroySpace) -> session
    // (xrDestroySession) -> binding (VkDevice, then VkInstance) ->
    // instance (xrDestroyInstance) last. Every GPU/OpenXR resource this
    // demo owns is destroyed while the handle it depends on
    // (bindingData.device/session/instance) is still alive, a direct
    // consequence of declaration order - verified against the actual
    // order above, not assumed.
}
