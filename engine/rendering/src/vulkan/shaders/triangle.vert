#version 450

// M8E: adds a UV attribute (location 2), passed through to the
// fragment shader for texture sampling. See docs/ARCHITECTURE.md,
// "Vertex Format (M8E)" - location 0/1/2 here must match
// VulkanVertex.cpp's GetAttributeDescriptions() exactly.

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUV;

void main()
{
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
    fragUV = inUV;
}
