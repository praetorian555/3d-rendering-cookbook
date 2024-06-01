#version 460

#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable

#include "material-data.glsl"

layout(std140, binding = 0) uniform PerFrameData
{
    mat4 view_projection_transform;
    vec3 camera_position_world;
};

layout(std430, binding = 3) restrict readonly buffer Materials
{
    MaterialData in_materials[];
};

layout (location = 0) in vec3 in_normal_world;
layout (location = 1) in vec2 in_tex_coords;
layout (location = 2) in vec3 in_position_world;
layout (location = 3) in flat uint in_material_index;

// Textures that are same for all materials
layout (binding = 5) uniform samplerCube tex_env_map;
layout (binding = 6) uniform samplerCube tex_env_map_irradiance;
layout (binding = 7) uniform sampler2D tex_brdf_lut;

layout (location = 0) out vec4 out_frag_color;

#include "pbr-shared.glsl"
#include "alpha-test.glsl"

void main()
{
    MaterialData mtl = in_materials[in_material_index];

    // Figure out albedo color
    vec4 albedo = mtl.albedo_color;
    if (mtl.albedo_map > 0)
    {
        albedo = texture(sampler2D(unpackUint2x32(mtl.albedo_map)), in_tex_coords);
    }

    // Figute out the normal
    vec3 normal_sample = vec3(0.0, 0.0, 0.0);
    if (mtl.normal_map > 0)
    {
        normal_sample = texture(sampler2D(unpackUint2x32(mtl.normal_map)), in_tex_coords).xyz;
    }

    // If alpha test fails, discard the fragment
    RunAlphaTest(albedo.a, mtl.alpha_test);

    vec3 normal_world = normalize(in_normal_world);
    if (length(normal_sample) > 0.5)
    {
        normal_world = PerturbNormal(normal_world, normalize(camera_position_world - in_position_world), normal_sample, in_tex_coords);
    }

    #ifdef USE_PBR
    vec4 metal_roughness_sample = mtl.roughness;
    metal_roughness_sample.b = mtl.metallic_factor;
    if (mtl.metallic_roughness_map > 0)
    {
        metal_roughness_sample = texture(sampler2D(unpackUint2x32(mtl.metallic_roughness_map)), in_tex_coords);
    }
    #endif

    #ifdef USE_AO
    // Ambient occlusion factor
    vec4 kao = vec4(0.0, 0.0, 0.0, 0.0);
    if (mtl.ambient_occlusion_map > 0)
    {
        kao = texture(sampler2D(unpackUint2x32(mtl.ambient_occlusion_map)), in_tex_coords);
    }
    #endif

    #ifdef USE_EMISSIVE
    // Emissive color factor
    vec4 ke  = mtl.emissive_color;
    if (mtl.emissive_map > 0)
    {
        ke = texture(sampler2D(unpackUint2x32(mtl.emissive_map)), in_tex_coords);
    }
    ke.rgb = SrgbToLinear(ke).rgb;
    #endif

    vec3 light_direction = normalize(vec3(-1.0, 1.0, 0.1));

    #ifdef USE_PBR
    PbrInfo pbr_inputs;
    // image-based lighting
    vec3 color = CalculatePBRInputsMetallicRoughness(albedo, normal_world, camera_position_world.xyz, in_position_world, metal_roughness_sample, pbr_inputs);
    // one hardcoded light source
    color += CalculatePBRLightContribution(pbr_inputs, light_direction, vec3(1.0));
    #else
    float n_dot_l = 1; // clamp(dot(normal_world, light_direction), 0.3, 1.0);
    vec3 color = albedo.rgb * n_dot_l;
    #endif

    #ifdef USE_AO
    // ambient occlusion
    color = color * (kao.r < 0.01 ? 1.0 : kao.r);
    #endif

    #ifdef USE_EMISSIVE
    // emissive
    color = pow(ke.rgb + color, vec3(1.0 / 2.2));
    #endif

    out_frag_color = vec4(color, albedo.a);
}
