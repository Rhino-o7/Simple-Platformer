#version 300 es
precision highp float;
precision highp int;

layout (location = 0) in vec3 vert_position;
layout (location = 2) in uint vert_material;

flat out uint frag_material;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

void main() {
    vec4 view_position = view * model * vec4(vert_position, 1.0);
    gl_Position = proj * view_position;
    frag_material = vert_material;
}
