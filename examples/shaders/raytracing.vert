#version 450
#include "../../src/shaders/common.glsl"

layout (location = 0) in vec3 pos;

layout (location = 0) out vec3 out_pos;
layout (location = 1) out vec2 out_coords;

void main() {
    gl_Position = vec4(pos, 1.0);
    out_coords = pos.xy;
    out_pos = pos;
}
