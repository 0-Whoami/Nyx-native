#version 450

layout(set=0, binding=0) uniform Uniform_data{ vec2 vertex[4]; } ubo;
layout(push_constant) uniform push_const{ vec2 offset;float texture_x; } push;
layout(location = 0) out vec2 uv;

void main() {
    vec2 pos=ubo.vertex[gl_VertexIndex];
    gl_Position = vec4(pos+push.offset, 0.0, 1.0);
    uv=vec2(pos.x+push.texture_x, pos.y);
}