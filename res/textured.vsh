#version 150 core

uniform mat4 mvp;

in vec2 in_position; 
// in vec4 in_normal; 
in vec2 in_texcoord; 
// in vec4 in_color; 


// out vec4 pass_Color;
out vec2 pass_texcoord;

void main(void) {
    // pass_Color = in_color;
    pass_texcoord = in_texcoord.st;
    gl_Position = mvp * vec4(in_position, 0.0, 1.0);
}