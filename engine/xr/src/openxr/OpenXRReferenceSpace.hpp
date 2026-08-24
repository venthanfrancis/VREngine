#pragma once

// Private OpenXR bring-up implementation — see OpenXRSession.hpp.
//
// Deliberately no Vulkan dependency: reference spaces are a pure
// OpenXR/tracking concept, independent of which graphics API backs the
// session. See docs/ARCHITECTURE.md, "Space Semantics (M9D)".

#include <openxr/openxr.h>

#include <vector>

namespace AREngine::XR::OpenXR
{
    // Makes a real xrEnumerateReferenceSpaces call - not unit-tested,
    // only exercised by the manual session demo. `session` must
    // already be created.
    [[nodiscard]] std::vector<XrReferenceSpaceType> EnumerateReferenceSpaceTypes(XrInstance instance, XrSession session);

    // Pure logic over already-queried data - directly unit-testable.
    [[nodiscard]] bool IsReferenceSpaceTypeSupported(
        const std::vector<XrReferenceSpaceType>& supported, XrReferenceSpaceType type);

    // VIEW and LOCAL are the spaces M9D always wants when the runtime
    // supports them (never assumed - each is checked against
    // `supported` individually). STAGE is included too when supported,
    // for diagnostics only - AREngine does not require STAGE to exist.
    // Pure logic, directly unit-testable. See docs/ARCHITECTURE.md,
    // "Reference Spaces (M9D)".
    [[nodiscard]] std::vector<XrReferenceSpaceType> SelectReferenceSpacesToCreate(
        const std::vector<XrReferenceSpaceType>& supported);

    // Identity pose (no rotation, no positional offset) - used for
    // poseInReferenceSpace on every space M9D creates. No deliberate
    // reason yet to use anything else; XrQuaternionf{0,0,0,1} is the
    // identity rotation (w=1), XrVector3f{0,0,0} is no offset.
    [[nodiscard]] constexpr XrPosef IdentityPose()
    {
        return XrPosef{ XrQuaternionf{0.0f, 0.0f, 0.0f, 1.0f}, XrVector3f{0.0f, 0.0f, 0.0f} };
    }

    // Owns one XrSpace. Not copyable or movable: exactly one XrSpace
    // per OpenXRReferenceSpace, destroyed exactly once (xrDestroySpace),
    // by this object alone - same discipline as every other owned
    // handle in this engine's bring-up code. Callers are responsible
    // for destroying every OpenXRReferenceSpace before the XrSession
    // (and OpenXRSession) it was created from - see
    // docs/ARCHITECTURE.md, "Destruction Order (M9D)".
    class OpenXRReferenceSpace
    {
    public:
        OpenXRReferenceSpace(XrInstance instance, XrSession session, XrReferenceSpaceType type,
                              XrPosef poseInReferenceSpace = IdentityPose());
        ~OpenXRReferenceSpace();

        OpenXRReferenceSpace(const OpenXRReferenceSpace&) = delete;
        OpenXRReferenceSpace& operator=(const OpenXRReferenceSpace&) = delete;
        OpenXRReferenceSpace(OpenXRReferenceSpace&&) = delete;
        OpenXRReferenceSpace& operator=(OpenXRReferenceSpace&&) = delete;

        [[nodiscard]] XrSpace Get() const { return m_space; }
        [[nodiscard]] XrReferenceSpaceType GetType() const { return m_type; }

    private:
        XrSpace m_space = XR_NULL_HANDLE;
        XrReferenceSpaceType m_type;
    };
}
