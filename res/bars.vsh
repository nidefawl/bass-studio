#version 150 core

uniform int u_pass;
uniform float u_time;
uniform mat4 u_mvp;
uniform mat4 u_modelView;
uniform float u_numbands;
uniform vec2 u_viewport;

in vec2 a_position;
in vec2 a_texcoord;
in vec4 a_color;
in vec4 a_vattr;
in vec4 a_vattr2;

out vec2 pass_texcoord;
out vec4 pass_color;
out vec4 pass_attr;
out vec4 pass_attr2;
flat out vec2 barSize;

mat3 rotationMatrix(vec3 axis, float angle)
{
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    return mat3(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c          );
}
void main(void) {
    pass_texcoord = a_texcoord;
    pass_color = a_color;
    pass_attr = a_vattr;
    pass_attr2 = a_vattr2;
    barSize = pass_attr2.xy;
    float sign = 1.0 - u_pass * 2.0;

    vec3 pos = vec3(a_position.xy, 0.0);
    // vec4 posEyeSpace = (u_modelView * vec4(pos.xyz, 1.0));

    // float posXRel = abs((posEyeSpace.x/u_viewport.x)*2.0);

    // posXRel*=posXRel;
    // posXRel = 1.0-posXRel;
    // posEyeSpace.z -= posXRel*100.0;


    // gl_Position = u_mvp * posEyeSpace;

    // float posXRel = abs((a_position.x/u_viewport.x)*2.0-1.0);
    // posXRel = 1.0-sqrt(1.0-posXRel*posXRel);
    // float f = 200;
    // pos.z += (posXRel*-f + f - 80)*sign;
    gl_Position = u_mvp * u_modelView * vec4(pos, 1.0);
}