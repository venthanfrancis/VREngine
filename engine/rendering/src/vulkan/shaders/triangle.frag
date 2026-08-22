#version 450

// M8E: samples a real 2D texture at the interpolated UV and multiplies
// it by the interpolated per-vertex color - proves both the vertex-
// color path (M8D) and the new texture-sampling path (M8E) compose
// correctly in one draw. See docs/ARCHITECTURE.md, "Shader Sampling
// Path (M8E)".

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUV;

layout(set = 0, binding = 0) uniform sampler2D uTexture;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(uTexture, fragUV) * vec4(fragColor, 1.0);
}
