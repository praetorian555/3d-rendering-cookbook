#version 460 core

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_tex_coord;
layout (location = 3) in mat4 in_world_from_model;

layout (location = 0) out vec3 out_normal;
layout (location = 1) out vec2 out_tex_coord;

layout (std140, binding = 0) uniform Matrices {
    mat4 clip_space_from_world;
};

void main()
{
    gl_Position = clip_space_from_world * in_world_from_model * vec4(in_pos, 1.0);
    out_normal = in_normal;
    out_tex_coord = in_tex_coord;
}
