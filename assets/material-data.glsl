struct MaterialData
{
    vec4 emissive_color;
    vec4 albedo_color;
    vec4 roughness;

    float transparency_factor;
    float alpha_test;
    float metallic_factor;

    uint flags;

    uint64_t ambient_occlusion_map;
    uint64_t emissive_map;
    uint64_t albedo_map;
    uint64_t metallic_roughness_map;
    uint64_t normal_map;
    uint64_t opacity_map;
};