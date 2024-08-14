//
#version 460 core

layout(std140, binding = 0) uniform PerFrameData
{
    mat4 clip_from_world;
    mat4 light_clip_from_world;
    vec4 world_camera_position;
    vec4 light_angles;
    vec4 world_light_position;
};

struct PerVertex
{
    vec2 uv;
    vec4 shadow_coord;
    vec3 vertex_world_position;
};

layout (location=0) in PerVertex vtx;

layout (location=0) out vec4 out_frag_color;

layout (binding = 0) uniform sampler2D albedo_texture;
layout (binding = 1) uniform sampler2D shadow_texture;

// Percentage Closer Filtering
// kernel_size should be an odd number and it defines an averaging square in texels
float PCF(int kernel_size, vec2 shadow_coord, float depth)
{
    float size = 1.0 / float(textureSize(shadow_texture, 0).x);
    float shadow = 0.0;
    int range = kernel_size / 2;
    // We go over all the texels in the kernel and check if the depth is less than the current depth
    // If it is, we increment the shadow value by 1 (we are using a 0-1 range)
    for (int v = -range; v <= range; v++)
    {
        for (int u = -range; u <= range; u++)
        {
            // depth is the depth of the current fragment in this scene, we compare it with the depth in the shadow map
            // We use the red channel of the shadow map as the depth value
            shadow += (depth >= texture(shadow_texture, shadow_coord + size * vec2(u, v)).r) ? 1.0 : 0.0;
        }
    }
    return shadow / (kernel_size * kernel_size);
}

float ShadowFactor(vec4 shadow_coord)
{
    // Do perspective divide to get to NDC space
    vec4 shadow_coords4 = shadow_coord / shadow_coord.w;
    if (shadow_coords4.z > -1.0 && shadow_coords4.z < 1.0)
    {
        float depth_bias = -0.001; // Used to avoid self-shadowing 
        float shadow_sample = PCF(13, shadow_coords4.xy, shadow_coords4.z + depth_bias);
        return mix(1.0, 0.3, shadow_sample);
    }
    return 1.0;
}

// Spot-light shadowing coefficient calculation
float LightFactor(vec3 world_position)
{
    vec3 light_direction = normalize(world_light_position.xyz - world_position);
    vec3 sopt_direction  = normalize(-world_light_position.xyz);// light is always looking at (0, 0, 0)

    float rho = dot(-light_direction, sopt_direction);

    float outer_angle = light_angles.x;
    float inner_angle = light_angles.y;

    if (rho > outer_angle)
    {
        return smoothstep(outer_angle, inner_angle, rho);
    }
    return 0.0;
}

void main()
{
    vec3 albedo = texture(albedo_texture, vtx.uv).xyz;
    out_frag_color = vec4(albedo * ShadowFactor(vtx.shadow_coord) * LightFactor(vtx.vertex_world_position), 1.0);
}
