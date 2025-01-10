#version 450
#define DIM 1 << 1
#define INVISIBLE 1 << 6

layout(location=0) in vec2 vertex;
layout(location = 1) in int packed_data;

layout(location = 0) out vec2 texture_uv;
layout(location = 1) out flat vec3 fg;
layout(location = 2) out flat vec3 bg;
layout(location = 3) out flat int style;

layout(binding=0) uniform Info{
    vec3 colors[256];
    ivec2 row_col;
    vec2 texture_rect;
    vec2 offset;
    float zoom;
} ub;

void main() {
    int flags=packed_data&0xFF;
    if ((flags&INVISIBLE)!=0) gl_Position=vec4(1e10, 1e10, 1e10, 1.0);//discard
    fg=ub.colors[(packed_data>>8)&0xFF];
    bg=ub.colors[(packed_data>>16)&0xFF];
    gl_Position=vec4(0.0, 0.0, 0.0, 0.0);
}