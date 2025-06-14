#version 450
precision mediump float;
#define get(pos) ((packed_data&pos)!=0)
#define DIM get(1<<1)
#define ITALIC get(1<<2)
#define INVERSE get(1<<5)
#define INVISIBLE get(1<<6)
#define skew 0.1
#define c(n) vec3((n&0xFF0000)>>16, (n&0xFF00)>>8, n&0xFF)/255.0

layout(location=0)in int packed_data;

uniform vec3 mod;

layout(location=0) out vec2 uv;
layout(location=1) out vec3 fg;
layout(location=2) out vec3 bg;
layout(location=3) out bvec3 style;

const vec3 lut[256]=vec3[256](c(0), c(0xcd0000), c(0xcd00), c(0xcdcd00), c(0x6495ed), c(0xcd00cd), c(0xcdcd), c(0xe5e5e5), c(0x7f7f7f), c(0xff0000), c(0xff00), c(0xffff00), c(0x5c5cff), c(0xff00ff), c(0xffff), c(0xffffff), c(0), c(0x5f), c(0x87), c(0xaf), c(0xd7), c(0xff), c(0x5f00), c(0x5f5f), c(0x5f87), c(0x5faf), c(0x5fd7), c(0x5fff), c(0x8700), c(0x875f), c(0x8787), c(0x87af), c(0x87d7), c(0x87ff), c(0xaf00), c(0xaf5f), c(0xaf87), c(0xafaf), c(0xafd7), c(0xafff), c(0xd700), c(0xd75f), c(0xd787), c(0xd7af), c(0xd7d7), c(0xd7ff), c(0xff00), c(0xff5f), c(0xff87), c(0xffaf), c(0xffd7), c(0xffff), c(0x5f0000), c(0x5f005f), c(0x5f0087), c(0x5f00af), c(0x5f00d7), c(0x5f00ff), c(0x5f5f00), c(0x5f5f5f), c(0x5f5f87), c(0x5f5faf), c(0x5f5fd7), c(0x5f5fff), c(0x5f8700), c(0x5f875f), c(0x5f8787), c(0x5f87af), c(0x5f87d7), c(0x5f87ff), c(0x5faf00), c(0x5faf5f), c(0x5faf87), c(0x5fafaf), c(0x5fafd7), c(0x5fafff), c(0x5fd700), c(0x5fd75f), c(0x5fd787), c(0x5fd7af), c(0x5fd7d7), c(0x5fd7ff), c(0x5fff00), c(0x5fff5f), c(0x5fff87), c(0x5fffaf), c(0x5fffd7), c(0x5fffff), c(0x870000), c(0x87005f), c(0x870087), c(0x8700af), c(0x8700d7), c(0x8700ff), c(0x875f00), c(0x875f5f), c(0x875f87), c(0x875faf), c(0x875fd7), c(0x875fff), c(0x878700), c(0x87875f), c(0x878787), c(0x8787af), c(0x8787d7), c(0x8787ff), c(0x87af00), c(0x87af5f), c(0x87af87), c(0x87afaf), c(0x87afd7), c(0x87afff), c(0x87d700), c(0x87d75f), c(0x87d787), c(0x87d7af), c(0x87d7d7), c(0x87d7ff), c(0x87ff00), c(0x87ff5f), c(0x87ff87), c(0x87ffaf), c(0x87ffd7), c(0x87ffff), c(0xaf0000), c(0xaf005f), c(0xaf0087), c(0xaf00af), c(0xaf00d7), c(0xaf00ff), c(0xaf5f00), c(0xaf5f5f), c(0xaf5f87), c(0xaf5faf), c(0xaf5fd7), c(0xaf5fff), c(0xaf8700), c(0xaf875f), c(0xaf8787), c(0xaf87af), c(0xaf87d7), c(0xaf87ff), c(0xafaf00), c(0xafaf5f), c(0xafaf87), c(0xafafaf), c(0xafafd7), c(0xafafff), c(0xafd700), c(0xafd75f), c(0xafd787), c(0xafd7af), c(0xafd7d7), c(0xafd7ff), c(0xafff00), c(0xafff5f), c(0xafff87), c(0xafffaf), c(0xafffd7), c(0xafffff), c(0xd70000), c(0xd7005f), c(0xd70087), c(0xd700af), c(0xd700d7), c(0xd700ff), c(0xd75f00), c(0xd75f5f), c(0xd75f87), c(0xd75faf), c(0xd75fd7), c(0xd75fff), c(0xd78700), c(0xd7875f), c(0xd78787), c(0xd787af), c(0xd787d7), c(0xd787ff), c(0xd7af00), c(0xd7af5f), c(0xd7af87), c(0xd7afaf), c(0xd7afd7), c(0xd7afff), c(0xd7d700), c(0xd7d75f), c(0xd7d787), c(0xd7d7af), c(0xd7d7d7), c(0xd7d7ff), c(0xd7ff00), c(0xd7ff5f), c(0xd7ff87), c(0xd7ffaf), c(0xd7ffd7), c(0xd7ffff), c(0xff0000), c(0xff005f), c(0xff0087), c(0xff00af), c(0xff00d7), c(0xff00ff), c(0xff5f00), c(0xff5f5f), c(0xff5f87), c(0xff5faf), c(0xff5fd7), c(0xff5fff), c(0xff8700), c(0xff875f), c(0xff8787), c(0xff87af), c(0xff87d7), c(0xff87ff), c(0xffaf00), c(0xffaf5f), c(0xffaf87), c(0xffafaf), c(0xffafd7), c(0xffafff), c(0xffd700), c(0xffd75f), c(0xffd787), c(0xffd7af), c(0xffd7d7), c(0xffd7ff), c(0xffff00), c(0xffff5f), c(0xffff87), c(0xffffaf), c(0xffffd7), c(0xffffff), c(0x080808), c(0x121212), c(0x1c1c1c), c(0x262626), c(0x303030), c(0x3a3a3a), c(0x444444), c(0x4e4e4e), c(0x585858), c(0x626262), c(0x6c6c6c), c(0x767676), c(0x808080), c(0x8a8a8a), c(0x949494), c(0x9e9e9e), c(0xa8a8a8), c(0xb2b2b2), c(0xbcbcbc), c(0xc6c6c6), c(0xd0d0d0), c(0xdadada), c(0xe4e4e4), c(0xeeeeee));
const vec2 cell_rect=vec2(2.0/64.0, 2.0/32.0);
const vec2 coord[6] = vec2[6](
vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
vec2(1.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 1.0)
);

void main(){
    if (INVISIBLE){
        gl_Position=vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }
    fg=lut[(packed_data>>16)&0xff];
    bg=lut[(packed_data>>8)&0xff];
    if (INVERSE){
        vec3 tmp=fg;
        fg=bg;
        bg=tmp;
    }
    if (DIM){
        fg/=3.0;
        bg/=3.0;
    }
    uv=coord[gl_VertexIndex];
    gl_Position=vec4(-1, -1, 0, 0)+vec4((vec2(gl_InstanceID&63, gl_InstanceID>>6)+uv+mod.xy)*cell_rect*mod.z, 0.0, 0.0);
    uv.x=(((packed_data>>24)-33)+uv.x)*(1.0/120.0);
    if (ITALIC)uv.x+=skew*(int(uv.x)^int(uv.y))*(1-2*uv.y);
    style=bvec3(get(1<<0), get(1<<3), get(1<<7));
}