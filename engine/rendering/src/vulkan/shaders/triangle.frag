#version 450

// M8F adds `pc.tint`, multiplied in alongside the M8E texture sample
// and M8D vertex-color gradient - this is what visually distinguishes
// the "near" quad from the "far" quad in the depth-testing proof (see
// docs/ARCHITECTURE.md, "Exact Visual Proof That Depth Testing Works
// (M8F)") without needing two separate vertex buffers.

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUV;

layout(set = 0, binding = 0) uniform sampler2D uTexture;

layout(push_constant) uniform PushConstants
{
    mat4 mvp;
    vec4 tint;
} pc;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(uTexture, fragUV) * vec4(fragColor, 1.0) * pc.tint;
}
