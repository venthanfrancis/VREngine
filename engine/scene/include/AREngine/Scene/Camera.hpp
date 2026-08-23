#pragma once

#include "AREngine/Core/Math/Mat4.hpp"
#include "AREngine/Core/Math/ViewProjection.hpp"
#include "AREngine/Scene/Transform.hpp"

namespace AREngine::Scene
{
    // A camera's projection parameters — vertical field of view, near/
    // far planes, aspect ratio — deliberately decoupled from its pose.
    // Camera does NOT own or duplicate position/rotation: those live in
    // whatever Transform the caller supplies to GetViewMatrix(), the
    // same way every other piece of AREngine avoids storing the same
    // data twice. See docs/ARCHITECTURE.md, "Camera Ownership / Module
    // Placement (M8G)" for why this lives in Scene (not Core, which
    // provides only math primitives/helpers; not a Vulkan-specific
    // type — there is no VulkanCamera).
    //
    // Backend-independent: GetProjectionMatrix() returns
    // Core::Math::PerspectiveRH_ZO's plain right-handed, zero-to-one-
    // depth matrix — no Vulkan Y-flip. A renderer that needs one (as
    // Vulkan does) applies it itself, in its own layer — see
    // docs/ARCHITECTURE.md, "Projection Ownership (M8G)". No VkDevice,
    // VkBuffer, VkDescriptorSet, or any other Vulkan type appears
    // anywhere in this class.
    //
    // Deliberately small: no frustum culling, no orthographic mode, no
    // jitter, no stereo views, no camera stack, no post-processing
    // settings. See docs/ARCHITECTURE.md, "What's Deferred to M8H+
    // (M8G)".
    struct Camera
    {
        // ~60 degrees — a reasonable general-purpose default, matching
        // the fixed FOV M8F's demo already used.
        float verticalFovRadians = 1.0471975512f; // pi/3
        float nearZ = 0.1f;
        float farZ = 100.0f;

        // Builds the view matrix for `transform` — i.e. the matrix that
        // expresses world-space points relative to a camera positioned
        // and oriented as `transform` describes. Reuses
        // Core::Math::LookAtRH directly: the "target" it needs is just
        // one meter along `transform`'s own forward direction, so no
        // separate quaternion-to-view-matrix formula was needed — see
        // docs/ARCHITECTURE.md, "View-Matrix Generation (M8G)".
        [[nodiscard]] Core::Math::Mat4 GetViewMatrix(const Transform& transform) const
        {
            return Core::Math::LookAtRH(
                transform.position, transform.position + transform.GetForward(), Core::Math::kWorldUp);
        }

        [[nodiscard]] Core::Math::Mat4 GetProjectionMatrix() const
        {
            return Core::Math::PerspectiveRH_ZO(verticalFovRadians, m_aspectRatio, nearZ, farZ);
        }

        // Recomputes the projection on the next GetProjectionMatrix()
        // call. Intended to be called on every resize (and is harmless
        // to call every frame, if that's simpler for the caller — it's
        // just a float assignment). See docs/ARCHITECTURE.md, "How
        // Resize Changes Projection (M8G)".
        void SetAspectRatio(float widthOverHeight) { m_aspectRatio = widthOverHeight; }
        [[nodiscard]] float GetAspectRatio() const { return m_aspectRatio; }

    private:
        float m_aspectRatio = 16.0f / 9.0f; // overwritten by SetAspectRatio before first real use
    };
}
