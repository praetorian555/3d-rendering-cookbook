#version 460 core

layout (location = 0) in vec3 normal;
layout (location = 1) in vec2 tex_coord;

out vec4 out_frag_color;

uniform sampler2D tex;
vec3 light_pos = vec3(4.0, 5.0, -3.0);
vec3 light_color = vec3(1.0, 1.0, 1.0);

void main()
{
    float light_angle = max(dot(normalize(normal), normalize(light_pos)), 0.0);
    out_frag_color = texture(tex, tex_coord) * vec4((0.3 + 0.7 * light_angle) * light_color, 1.0);
}