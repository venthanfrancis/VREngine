// Manual M9E/M9E.5 validation demo — NOT part of the automated CTest
// suite, since it requires a real OpenXR loader/runtime with
// XR_KHR_vulkan_enable2 support (and ideally a real or simulated HMD)
// that CI/headless systems may lack. Built by CMake but deliberately
// not registered with add_test. Run it manually.
//
// Proves AREngine's first real OpenXR frame lifecycle end to end: create
// the M9C Vulkan graphics binding -> create an XrSession (M9D) -> select
// PRIMARY_STEREO (M9D) -> enumerate + select an environment blend mode
// -> enumerate + select a swapchain color format -> create one XrSwapchain
// per view, sized from the runtime's own recommended dimensions/sample
// count -> drive a real xrWaitFrame/xrBeginFrame/(acquire/wait/clear/
// release)/xrEndFrame loop THROUGH XRFrameDriver (M9E.5's generic
// Frame::FrameDriver implementation for OpenXR - see
// engine/xr/src/openxr/XRFrameDriver.hpp), submitting ZERO composition
// layers, until a target frame count is reached -> xrRequestExitSession
// -> observe the runtime drive STOPPING/EXITING -> clean shutdown.
//
// M9E.5 note: the session-state event loop (poll/BeginSession/EndSession/
// stop-detection) and the raw xrWaitFrame/xrBeginFrame/xrEndFrame calls
// that M9E drove manually, inline, in this file are now entirely inside
// XRFrameDriver - this demo only calls PrepareFrame()/BeginFrame()/
// EndFrame() and reacts to the generic FrameContext/FrameStatus/
// shouldRender they return. What stays here, unchanged in substance: the
// swapchain-specific Vulkan work (acquire/wait/clear/release across the
// two OpenXRSwapchains) - a deliberate ownership decision, not an
// oversight; see docs/ARCHITECTURE.md, "Render-Target Acquisition
// Ownership (M9E.5)". This demo plays the "Renderer/XR integration"
// coordinating role no dedicated module implements yet.
//
// Does NOT call xrLocateViews or xrLocateSpace (no real view pose/FOV
// data exists yet - deferred to M9F), does NOT submit any
// XrCompositionLayerProjection (submitting one would require exactly the
// xrLocateViews data this milestone deliberately does not fabricate),
// does NOT render any scene content (each eye's swapchain image is
// cleared to a solid, distinct color - proof that the OpenXR-owned
// VkImages are genuinely usable by AREngine's own Vulkan commands,
// nothing more), does NOT create any reference space (unlike M9D's demo -
// with zero composition layers submitted, nothing in M9E's frame loop
// actually needs an XrSpace; see docs/ARCHITECTURE.md, "No Reference
// Space Needed (M9E)"). See docs/ROADMAP.md.
//
// This demo reaches directly into XR's private src/openxr/
// implementation, same reasoning as M9A/M9C/M9D's demos.
//
// Same outcome-distinguishing discipline established since M9A: no
// runtime, no HMD system, no stereo view configuration, and (new this
// milestone) no supported environment blend mode or swapchain format are
// all reported clearly and stopped on cleanly, never crashed on.

#include "AREngine/Core/Core.hpp"
#include "AREngine/Frame/Frame.hpp"

#include "openxr/OpenXREnvironmentBlendMode.hpp"
#include "openxr/OpenXRInstance.hpp"
#include "openxr/OpenXRResult.hpp"
#include "openxr/OpenXRSession.hpp"
#include "openxr/OpenXRSwapchain.hpp"
#include "openxr/OpenXRSystem.hpp"
#include "openxr/OpenXRVersion.hpp"
#include "openxr/OpenXRViewConfiguration.hpp"
#include "openxr/OpenXRVulkanGraphicsBinding.hpp"
#include "openxr/XRFrameDriver.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

namespace
{
    // Small, self-contained CheckVkResult-equivalent - deliberately
    // duplicated rather than reused across modules, same discipline
    // OpenXRVulkanGraphicsBinding.cpp already established for this
    // exact reason (see its own copy of this helper).
    void CheckVkResultHere(VkResult result, const char* operation)
    {
        if (result != VK_SUCCESS)
        {
            const std::string message = std::format("{} failed: VkResult({})", operation, static_cast<int>(result));
            AR_LOG_ERROR(message);
            AR_ASSERT_MSG(false, message.c_str());
        }
    }

    // Demo-local layout transition helper - deliberately a fresh,
    // self-contained copy of the same idea
    // Rendering::Vulkan::TransitionImageLayout already implements
    // (VulkanImageLayoutTransition.cpp), not a shared/imported
    // function. XR does not include Rendering's private headers (same
    // reasoning FindGraphicsQueueFamily's duplication documents in
    // OpenXRVulkanGraphicsBinding.hpp), and the two transitions M9E
    // actually needs are different from Rendering's own two - so this
    // is its own minimal, purpose-built version, not a generalized
    // utility.
    void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags srcStage = 0;
        VkPipelineStageFlags dstStage = 0;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        {
            // COLOR_ATTACHMENT_OPTIMAL: the conventional layout a color
            // swapchain image is expected to be left in for the
            // compositor - even though M9E submits zero composition
            // layers this frame (so nothing actually reads this image
            // this frame), leaving it in the layout the runtime's own
            // usage flags (COLOR_ATTACHMENT_BIT) imply is the more
            // honest choice than leaving it in TRANSFER_DST_OPTIMAL.
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else
        {
            AR_ASSERT_MSG(false, "TransitionImageLayout only supports UNDEFINED->TRANSFER_DST_OPTIMAL and TRANSFER_DST_OPTIMAL->COLOR_ATTACHMENT_OPTIMAL");
        }

        vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // Demo-only formatting, not part of the reusable OpenXRSwapchain
    // library surface - same "small integration layer, not a bigger
    // reusable surface the library itself doesn't need" reasoning as
    // M9C/M9D's demo-local ToString helpers.
    std::string SwapchainFormatToString(std::int64_t format)
    {
        switch (static_cast<VkFormat>(format))
        {
            case VK_FORMAT_B8G8R8A8_SRGB: return "VK_FORMAT_B8G8R8A8_SRGB";
            case VK_FORMAT_R8G8B8A8_SRGB: return "VK_FORMAT_R8G8B8A8_SRGB";
            default:                      return std::format("VkFormat({})", format);
        }
    }

    // One distinct solid clear color per eye - proof that each acquired
    // swapchain image is genuinely, independently usable by AREngine's
    // own Vulkan commands (not, e.g., the same underlying image
    // returned twice by mistake). Index 0 = left/first view (a warm
    // red), index 1 = right/second view (a cool blue). Deliberately not
    // tied to any real scene content - see this file's header comment.
    constexpr std::array<VkClearColorValue, 2> kEyeClearColors{
        VkClearColorValue{{0.75f, 0.1f, 0.1f, 1.0f}},
        VkClearColorValue{{0.1f, 0.1f, 0.75f, 1.0f}},
    };

    // Within the milestone's specified 120-300 range.
    constexpr std::uint32_t kTargetFrameCount = 200;

    // Generous safety ceiling, same "manual-test convenience, not part
    // of OpenXR's own lifecycle" reasoning as M9D's kMaxDemoDuration -
    // just longer, since M9E must complete kTargetFrameCount real
    // xrWaitFrame/xrEndFrame round trips, not just a handful of session-
    // state events.
    constexpr std::chrono::seconds kMaxDemoDuration{120};
}

int main()
{
    using namespace AREngine::XR::OpenXR;
    namespace Frame = AREngine::Frame;

    AR_LOG_INFO(std::format("AREngine OpenXR frame demo - header version {}, requesting API version {}",
                             FormatXrVersion(XR_CURRENT_API_VERSION), FormatXrVersion(kTargetApiVersion)));

    // --- Instance extensions / instance creation / system selection /
    // view configuration selection: identical requirements to M9D - see
    // that milestone's demo for the full reasoning behind each stop
    // condition. ---
    const std::vector<XrExtensionProperties> extensions = EnumerateInstanceExtensions();
    if (!IsExtensionSupported(extensions, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME))
    {
        AR_LOG_WARNING(std::format("{} is NOT supported by the active OpenXR runtime - stopping here.",
                                    XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME));
        AR_LOG_INFO("OpenXR frame demo exiting cleanly (XR_KHR_vulkan_enable2 unavailable)");
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
        AR_LOG_INFO("OpenXR frame demo exiting cleanly (no instance available)");
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
        AR_LOG_INFO("OpenXR frame demo exiting cleanly (runtime present, no HMD system)");
        return 0;
    }
    AR_LOG_INFO(std::format("HMD system acquired: XrSystemId {}", systemResult.systemId));
    const XrSystemId systemId = systemResult.systemId;

    const std::vector<XrViewConfigurationType> viewConfigTypes = EnumerateViewConfigurationTypes(instance.Get(), systemId);
    const std::optional<XrViewConfigurationType> primaryViewConfigType = SelectPrimaryViewConfigurationType(viewConfigTypes);
    if (!primaryViewConfigType.has_value())
    {
        AR_LOG_WARNING("XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO is NOT supported by this runtime/system - stopping here.");
        AR_LOG_INFO("OpenXR frame demo exiting cleanly (no stereo view configuration)");
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

    // --- Environment blend mode: enumerate, then select. Never
    // hard-coded - see OpenXREnvironmentBlendMode.hpp. ---
    const std::vector<XrEnvironmentBlendMode> supportedBlendModes =
        EnumerateEnvironmentBlendModes(instance.Get(), systemId, *primaryViewConfigType);
    AR_LOG_INFO(std::format("Supported environment blend modes: {}", supportedBlendModes.size()));
    for (const XrEnvironmentBlendMode mode : supportedBlendModes)
    {
        AR_LOG_INFO(std::format("  {}", EnvironmentBlendModeToString(mode)));
    }
    const std::optional<XrEnvironmentBlendMode> selectedBlendMode = SelectEnvironmentBlendMode(supportedBlendModes);
    if (!selectedBlendMode.has_value())
    {
        AR_LOG_WARNING("None of OPAQUE/ALPHA_BLEND/ADDITIVE is supported by this runtime - stopping here.");
        AR_LOG_INFO("OpenXR frame demo exiting cleanly (no usable environment blend mode)");
        return 0;
    }
    AR_LOG_INFO(std::format("Selected environment blend mode: {}", EnvironmentBlendModeToString(*selectedBlendMode)));

    // --- M9C graphics binding + M9D session (unchanged) ---
    AR_LOG_INFO("Creating OpenXR-compatible Vulkan instance/device via XR_KHR_vulkan_enable2...");
    OpenXRVulkanGraphicsBinding binding(instance.Get(), systemId);
    AR_LOG_INFO(std::format("Vulkan API version selected: {}", FormatVkApiVersion(binding.GetSelectedVulkanApiVersion())));

    OpenXRSession session(instance.Get(), systemId, binding.GetBindingData());
    AR_LOG_INFO("XrSession created successfully");

    // --- M9E: swapchain format selection ---
    const std::vector<std::int64_t> supportedFormats = EnumerateSwapchainFormats(instance.Get(), session.Get());
    AR_LOG_INFO(std::format("Supported swapchain formats: {}", supportedFormats.size()));
    for (const std::int64_t format : supportedFormats)
    {
        AR_LOG_INFO(std::format("  {}", SwapchainFormatToString(format)));
    }
    const std::optional<std::int64_t> selectedFormat = SelectSwapchainColorFormat(supportedFormats);
    if (!selectedFormat.has_value())
    {
        AR_LOG_WARNING("The runtime reported zero swapchain formats - stopping here.");
        AR_LOG_INFO("OpenXR frame demo exiting cleanly (no usable swapchain format)");
        return 0;
    }
    AR_LOG_INFO(std::format("Selected swapchain format: {}", SwapchainFormatToString(*selectedFormat)));

    // --- M9E: one XrSwapchain per view (see docs/ARCHITECTURE.md, "XR
    // Swapchain Topology (M9E)" for why one-per-view was chosen over one
    // array swapchain). Declared AFTER `session` so they are destroyed
    // BEFORE it, same reverse-local-destruction-order discipline as
    // M9D's reference spaces. Dimensions/sample count come directly
    // from each view's own XrViewConfigurationView - never hard-coded. ---
    std::vector<std::unique_ptr<OpenXRSwapchain>> swapchains;
    swapchains.reserve(viewConfigViews.size());
    for (std::size_t i = 0; i < viewConfigViews.size(); ++i)
    {
        const XrViewConfigurationView& view = viewConfigViews[i];
        swapchains.push_back(std::make_unique<OpenXRSwapchain>(
            instance.Get(), session.Get(), *selectedFormat,
            view.recommendedImageRectWidth, view.recommendedImageRectHeight, view.recommendedSwapchainSampleCount));
        AR_LOG_INFO(std::format("Created swapchain for view {}: {}x{}, {} image(s)",
                                 i, swapchains.back()->GetWidth(), swapchains.back()->GetHeight(), swapchains.back()->GetImages().size()));
    }

    // --- Minimal Vulkan resources for the per-eye clear: one command
    // pool, one reusable command buffer, one fence. Destroyed explicitly
    // before this function returns (see the comment above the explicit
    // cleanup calls near the end) - these are raw Vulkan handles with no
    // RAII wrapper in this demo, same as the rest of this codebase's
    // manual demos (e.g. vulkan_present_demo.cpp's own fences). ---
    const VulkanGraphicsBindingData& bindingData = binding.GetBindingData();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = bindingData.queueFamilyIndex;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    CheckVkResultHere(vkCreateCommandPool(bindingData.device, &poolInfo, nullptr, &commandPool), "vkCreateCommandPool");

    VkCommandBufferAllocateInfo cbAllocInfo{};
    cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbAllocInfo.commandPool = commandPool;
    cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAllocInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    CheckVkResultHere(vkAllocateCommandBuffers(bindingData.device, &cbAllocInfo, &commandBuffer), "vkAllocateCommandBuffers");

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // Unsignaled: this demo waits on the fence only AFTER submitting
    // that same frame's work (fully synchronous, one frame's GPU work
    // at a time - see the wait call below), so there is never a "first
    // frame" wait against a fence nothing has signaled yet.
    VkFence renderFence = VK_NULL_HANDLE;
    CheckVkResultHere(vkCreateFence(bindingData.device, &fenceInfo, nullptr, &renderFence), "vkCreateFence");

    // --- Frame loop, driven through XRFrameDriver (M9E.5) ---
    AR_LOG_INFO(std::format("Beginning OpenXR frame loop - target {} completed frames before requesting exit...", kTargetFrameCount));

    XRFrameDriver frameDriver(instance.Get(), session, *primaryViewConfigType, *selectedBlendMode);

    std::uint32_t completedFrameCount = 0;
    bool exitRequested = false;
    const auto startTime = std::chrono::steady_clock::now();

    while (true)
    {
        // PrepareFrame() internally polls session-state events (reacting
        // to every transition observed, in order - not just the last
        // one) and either calls the real xrWaitFrame (FrameStatus::
        // Continue) or reports FrameStatus::Idle (session not currently
        // running - not yet begun, or between STOPPING and EXITING; no
        // xrWaitFrame call was made) or FrameStatus::Stop (session
        // reached a terminal state). See XRFrameDriver.cpp.
        const Frame::FrameContext frameContext = frameDriver.PrepareFrame();

        if (frameContext.status == Frame::FrameStatus::Stop)
        {
            AR_LOG_INFO("Frame driver reports FrameStatus::Stop - stopping the main loop cleanly (not an error)");
            break;
        }

        if (frameContext.status == Frame::FrameStatus::Idle)
        {
            // No BeginFrame()/EndFrame() call this tick at all - see
            // FrameStatus.hpp for why this is a different situation
            // from shouldRender=false below. XRFrameDriver itself
            // sleeps briefly in this branch to avoid a CPU hot-spin
            // (see its own PrepareFrame() comment) - this loop only
            // needs its own safety-timeout check here.
            if (std::chrono::steady_clock::now() - startTime > kMaxDemoDuration)
            {
                AR_LOG_WARNING("Safety timeout reached while the frame driver was idle - stopping.");
                break;
            }
            continue;
        }

        frameDriver.BeginFrame();

        if (frameContext.timing.shouldRender)
        {
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

            CheckVkResultHere(vkResetCommandBuffer(commandBuffer, 0), "vkResetCommandBuffer");
            VkCommandBufferBeginInfo cbBeginInfo{};
            cbBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            cbBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            CheckVkResultHere(vkBeginCommandBuffer(commandBuffer, &cbBeginInfo), "vkBeginCommandBuffer");

            VkImageSubresourceRange range{};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.baseMipLevel = 0;
            range.levelCount = 1;
            range.baseArrayLayer = 0;
            range.layerCount = 1;

            for (std::size_t i = 0; i < swapchains.size(); ++i)
            {
                const VkImage image = swapchains[i]->GetImages()[acquiredIndices[i]];
                TransitionImageLayout(commandBuffer, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                vkCmdClearColorImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      &kEyeClearColors[i % kEyeClearColors.size()], 1, &range);
                TransitionImageLayout(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            }

            CheckVkResultHere(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");

            CheckVkResultHere(vkResetFences(bindingData.device, 1, &renderFence), "vkResetFences");
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            CheckVkResultHere(vkQueueSubmit(binding.GetQueue(), 1, &submitInfo, renderFence), "vkQueueSubmit");

            // Wait for the GPU to finish before releasing the swapchain
            // images back to the compositor - xrReleaseSwapchainImage
            // must never be called while GPU work targeting that image
            // is still in flight. A fence (not vkQueueWaitIdle) waits
            // only for THIS frame's specific work, not the whole queue.
            // Synchronous - one frame's GPU work at a time, no
            // multi-frame-in-flight pipelining - adequate for M9E's
            // diagnostic clear, deliberately temporary; see
            // docs/ARCHITECTURE.md, "Synchronization Before Release (M9E)".
            CheckVkResultHere(
                vkWaitForFences(bindingData.device, 1, &renderFence, VK_TRUE, std::numeric_limits<std::uint64_t>::max()),
                "vkWaitForFences");

            for (std::size_t i = 0; i < swapchains.size(); ++i)
            {
                XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                CheckXrResult(instance.Get(), xrReleaseSwapchainImage(swapchains[i]->Get(), &releaseInfo), "xrReleaseSwapchainImage");
            }
        }

        // Zero composition layers submitted - see this file's header
        // comment for why (no real xrLocateViews pose/FOV data exists
        // yet). EndFrame() calls xrEndFrame with layerCount=0, which is
        // spec-legal and means "the compositor shows nothing new from
        // this application this frame" - the frame lifecycle mechanics
        // above (wait/begin/acquire/wait/clear/release/end) are fully
        // exercised regardless.
        frameDriver.EndFrame();

        ++completedFrameCount;
        if (completedFrameCount == 1 || completedFrameCount % 50 == 0)
        {
            AR_LOG_INFO(std::format("Completed frame {} (predictedDisplayTime {:.4f}s, shouldRender={})",
                                     completedFrameCount, frameContext.timing.predictedDisplayTimeSeconds,
                                     frameContext.timing.shouldRender));
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

    AR_LOG_INFO(std::format("Total completed frames: {}", completedFrameCount));

    // No post-loop EndSession fallback needed here (M9E's demo had one) -
    // XRFrameDriver's PrepareFrame() already reacts to every session-
    // state transition on every tick it runs, so by the time this loop
    // exits, BeginSession/EndSession have already been called correctly
    // wherever the observed states warranted it. The one remaining edge
    // case - loop exits via the safety timeout while the session is
    // still marked running, having never reached STOPPING - is handled
    // identically to before: destroying a still-running session directly
    // is spec-legal, and OpenXRSession's destructor (below) does exactly
    // that.

    // Explicit cleanup of the raw Vulkan handles this demo created
    // directly (no RAII wrapper for these in a manual demo - see their
    // creation comment above), in dependency order, BEFORE returning -
    // this must happen before `binding`'s destructor destroys the
    // VkDevice they belong to.
    vkDestroyFence(bindingData.device, renderFence, nullptr);
    vkDestroyCommandPool(bindingData.device, commandPool, nullptr); // also frees commandBuffer

    AR_LOG_INFO("OpenXR frame demo complete - shutting down");
    return 0;

    // Destruction, in order (see each declaration's comment above):
    // `swapchains` (each xrDestroySwapchain), then `session`
    // (xrDestroySession), then `binding` (VkDevice, then VkInstance),
    // then `instance` (xrDestroyInstance) last.
}
