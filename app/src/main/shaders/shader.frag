#version 450
layout(binding = 0) uniform sampler2D texture_atlus;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;
layout(push_constant) uniform Color_Pair {
    vec3 fg;
    vec3 bg;
} pc;

void main() {
    if (texture(texture_atlus, uv).r > 0.1){
        outColor = vec4(pc.fg, 1.0);
    } else {
        outColor=vec4(pc.bg,1.0);
    }
}