//
#version 460 core

layout(std140, binding = 0) uniform PerFrameData
{
    mat4 view_projection_matrix;
    vec4 camera_position_world;
};

layout (location=0) in vec3 normal;
layout (location=1) in vec2 tex_coords;
layout (location=2) in vec3 position_world;

layout (location=0) out vec4 out_frag_color;

layout (binding = 0) uniform sampler2D tex_ao;
layout (binding = 1) uniform sampler2D tex_emissive;
layout (binding = 2) uniform sampler2D tex_albedo;
layout (binding = 3) uniform sampler2D tex_metal_roughness;
layout (binding = 4) uniform sampler2D tex_normal;

layout (binding = 5) uniform samplerCube tex_env_map;
layout (binding = 6) uniform samplerCube tex_env_map_irradiance;
layout (binding = 7) uniform sampler2D tex_brdf_lut;

#include "pbr-shared.glsl"

void main()
{
    vec4 kao = texture(tex_ao, tex_coords);
    vec4 ke  = texture(tex_emissive, tex_coords);
    vec4 kd  = texture(tex_albedo, tex_coords);
    vec2 mer = texture(tex_metal_roughness, tex_coords).yz;

    // world-space normal
    vec3 n = normalize(normal);

    vec3 normal_sample = texture(tex_normal, tex_coords).xyz;

    // normal mapping
    vec3 view_vector = normalize(camera_position_world.xyz - position_world);
    n = PerturbNormal(n, view_vector, normal_sample, tex_coords);

    vec4 mr_sample = texture(tex_metal_roughness, tex_coords);

    PbrInfo pbr_inputs;
    ke.rgb = SrgbToLinear(ke).rgb;
    // image-based lighting
    vec3 color = CalculatePBRInputsMetallicRoughness(kd, n, camera_position_world.xyz, position_world, mr_sample, pbr_inputs);
    // one hardcoded light source
    color += CalculatePBRLightContribution( pbr_inputs, normalize(vec3(-1.0, -1.0, -1.0)), vec3(1.0) );
    // ambient occlusion
    color = color * (kao.r < 0.01 ? 1.0 : kao.r);
    // emissive
    color = pow(ke.rgb + color, vec3(1.0 / 2.2));

    out_frag_color = vec4(color, 1.0);
}
