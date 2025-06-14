#version 450
precision mediump float;
uniform sampler2D texture_atlus;
in vec3 fg;
in vec3 bg;
in vec2 uv;
in bvec3 style;
#define BOLD style.x
#define UNDERLINE ((uv.y<0.1)&&style.y)
#define STRIKETHROUGH ((uv.y>0.45)&&(uv.y<0.55)&&style.z)
void main(){
float a=texture(texture_atlus,uv).r;
if(BOLD)a+=fwidth(a)*2.0;
gl_FragColor=(UNDERLINE||STRIKETHROUGH)?vec4(fg,1.0):vec4(mix(bg,fg,a),1.0);
}