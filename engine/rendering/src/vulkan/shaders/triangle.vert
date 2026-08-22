#version 450

// M8D: reads real per-vertex data from a vertex buffer instead of
// generating positions from gl_VertexIndex. See docs/ARCHITECTURE.md,
// "Vertex Input Layout (M8D)" - location 0/1 here must match
// VulkanVertex.cpp's GetAttributeDescriptions() exactly.

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main()
{
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}
