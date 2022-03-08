#version 150 core

uniform mat4 u_mvp;

in vec2 in_position; 
// in vec4 in_normal; 
in vec2 in_texcoord; 
// in vec4 in_color; 


// out vec4 pass_Color;
out vec2 pass_texcoord;
out vec4 pass_position;

void main(void) {
    // pass_Color = in_color;
    pass_texcoord = in_texcoord.st;
    pass_position = u_mvp * vec4(in_position, 0.0, 1.0);
    gl_Position = pass_position;
}