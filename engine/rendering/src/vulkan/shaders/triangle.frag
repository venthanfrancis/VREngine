#version 450

// Outputs the per-vertex color triangle.vert produced, interpolated
// across the triangle by the rasterizer - proves the fixed-function
// interpolation stage between the two shaders is actually working, not
// just "a shape appeared."

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(fragColor, 1.0);
}
