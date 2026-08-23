#version 450

// M8F: position is now a real 3D coordinate (was vec2 through M8E),
// and gl_Position comes from a genuine Model/View/Projection transform
// instead of being copied straight through. The MVP matrix (already
// multiplied down to one mat4 on the CPU) and a per-draw tint color
// arrive via push constants - see docs/ARCHITECTURE.md, "Transform
// Upload Method (M8F)". Location 0/1/2 here must match
// VulkanVertex.cpp's GetAttributeDescriptions() exactly.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUV;

layout(push_constant) uniform PushConstants
{
    mat4 mvp;
    vec4 tint;
} pc;

void main()
{
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragUV = inUV;
}
