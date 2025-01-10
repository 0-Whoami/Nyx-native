#version 450

#define get(pos)        ((style&(pos))!=0)
#define BOLD            get(1 << 0)
#define ITALIC          get(1 << 2)
#define UNDERLINE       ((uv.y>0.9)&&get(1 << 3))
#define INVERSE         get(1 << 5)
#define STRIKETHROUGH   ((uv.y>0.45)&&(uv.y<0.55)&&get(1 << 7))

layout(binding = 0) uniform sampler2D texture_atlus;
layout(location = 0) in vec2 uv;
layout(location = 1) in flat vec3 fg;
layout(location = 2) in flat vec3 bg;
layout(location = 3) in flat int style;
layout(location = 0) out vec4 outColor;

void main() {
    vec2 skew = ITALIC? vec2((uv.x-0.1*uv.y), uv.y) : uv;
    float a=texture(texture_atlus, skew).r;
    if (BOLD) a+=fwidth(a)*2.0;
    outColor=((a>0.1||UNDERLINE||STRIKETHROUGH)&&!INVERSE)?vec4(fg, 1.0):vec4(bg, 1.0);
}