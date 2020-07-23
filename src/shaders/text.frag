#version 450

layout (binding = 2) uniform TextParams {
    ivec2 grid_size;  // (6, 16)
    ivec2 tex_size;  // (400, 200)
} params;

layout(binding = 3) uniform sampler2D tex_sampler;

layout(location = 0) in vec4 color;
layout(location = 1) in vec2 tex_coords;
layout(location = 2) in vec2 glyph_size;
layout(location = 3) in float str_index;

layout(location = 0) out vec4 out_color;

#include "text_functions.glsl"

void main() {
    #include "text_frag.glsl"
}
