// Based on: https://github.com/KhronosGroup/glTF-WebGL-PBR/blob/master/shaders/pbr-frag.glsl

// Encapsulate the various inputs used by the various functions in the shading equation
// We store values in this struct to simplify the integration of alternative implementations
// of the shading terms, outlined in the Readme.MD Appendix.
struct PbrInfo
{
	float n_dot_l;                   // cos angle between normal and light direction
	float n_dot_v;                   // cos angle between normal and view direction
	float n_dot_h;                   // cos angle between normal and half vector
	float l_dot_h;                   // cos angle between light direction and half vector
	float v_dot_h;                   // cos angle between view direction and half vector
	float perceptual_roughness;      // roughness value, as authored by the model creator (input to shader)
	vec3 reflectance0;               // full reflectance color (normal incidence angle)
	vec3 reflectance90;              // reflectance color at grazing angle
	float alpha_roughness;           // roughness mapped to a more linear change in the roughness (proposed by [2])
	vec3 diffuse_color;              // color contribution from diffuse lighting
	vec3 specular_color;             // color contribution from specular lighting
	vec3 normal;					 // normal at surface point
	vec3 vec_from_surface_to_camera; // vector from surface point to camera
};

const float M_PI = 3.141592653589793;

vec4 SrgbToLinear(vec4 in_srgb)
{
	vec3 out_lin = pow(in_srgb.xyz,vec3(2.2));

	return vec4(out_lin, in_srgb.a);
}

// Calculation of the lighting contribution from an optional Image Based Light source.
// Precomputed Environment Maps are required uniform inputs and are computed as outlined in [1].
// See our README.md on Environment Maps [3] for additional discussion.
vec3 GetIblContribution(PbrInfo pbr_inputs, vec3 n, vec3 reflection, samplerCube env_map, samplerCube env_map_irradiance, sampler2D brdf_lut)
{
	float mip_count = float(textureQueryLevels(tex_env_map));
	float lod = pbr_inputs.perceptual_roughness * mip_count;
	// retrieve a scale and bias to F0. See [1], Figure 3
	vec2 brdf_sample_point = clamp(vec2(pbr_inputs.n_dot_v, 1.0 - pbr_inputs.perceptual_roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
	vec3 brdf = textureLod(brdf_lut, brdf_sample_point, 0).rgb;
#ifdef VULKAN
	// convert cubemap coordinates into Vulkan coordinate space
	vec3 cm = vec3(-1.0, -1.0, 1.0);
#else
	vec3 cm = vec3(1.0, 1.0, 1.0);
#endif
	// HDR envmaps are already linear
	vec3 diffuse_light = texture(env_map_irradiance, n.xyz * cm).rgb;
	vec3 specular_light = textureLod(env_map, reflection.xyz * cm, lod).rgb;

	vec3 diffuse = diffuse_light * pbr_inputs.diffuse_color;
	vec3 specular = specular_light * (pbr_inputs.specular_color * brdf.x + brdf.y);

	return diffuse + specular;
}

// Disney Implementation of diffuse from Physically-Based Shading at Disney by Brent Burley. See Section 5.3.
// http://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
vec3 DiffuseBurley(PbrInfo pbr_inputs)
{
	float f90 = 2.0 * pbr_inputs.l_dot_h * pbr_inputs.l_dot_h * pbr_inputs.alpha_roughness - 0.5;

	return (pbr_inputs.diffuse_color / M_PI) * (1.0 + f90 * pow((1.0 - pbr_inputs.n_dot_l), 5.0)) * (1.0 + f90 * pow((1.0 - pbr_inputs.n_dot_v), 5.0));
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 SpecularReflection(PbrInfo pbr_inputs)
{
	return pbr_inputs.reflectance0 + (pbr_inputs.reflectance90 - pbr_inputs.reflectance0) * pow(clamp(1.0 - pbr_inputs.v_dot_h, 0.0, 1.0), 5.0);
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
// This implementation is based on [1] Equation 4, and we adopt their modifications to
// alpha_roughness as input as originally proposed in [2].
float GeometricOcclusion(PbrInfo pbr_inputs)
{
	float n_dot_l = pbr_inputs.n_dot_l;
	float n_dot_v = pbr_inputs.n_dot_v;
	float r_sqr = pbr_inputs.alpha_roughness * pbr_inputs.alpha_roughness;

	float attenuation_l = 2.0 * n_dot_l / (n_dot_l + sqrt(r_sqr + (1.0 - r_sqr) * (n_dot_l * n_dot_l)));
	float attenuation_v = 2.0 * n_dot_v / (n_dot_v + sqrt(r_sqr + (1.0 - r_sqr) * (n_dot_v * n_dot_v)));
	return attenuation_l * attenuation_v;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float MicrofacetDistribution(PbrInfo pbr_inputs)
{
	float roughness_sq = pbr_inputs.alpha_roughness * pbr_inputs.alpha_roughness;
	float f = (pbr_inputs.n_dot_h * roughness_sq - pbr_inputs.n_dot_h) * pbr_inputs.n_dot_h + 1.0;
	return roughness_sq / (M_PI * f * f);
}

vec3 CalculatePBRInputsMetallicRoughness(vec4 albedo, vec3 normal, vec3 camera_position_world, vec3 position_world, vec4 mr_sample, out PbrInfo pbr_inputs)
{
	float perceptual_roughness = 1.0;
	float metallic = 1.0;

	// Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
	// This layout intentionally reserves the 'r' channel for (optional) occlusion map data
	perceptual_roughness = mr_sample.g * perceptual_roughness;
	metallic = mr_sample.b * metallic;

	const float c_min_roughness = 0.04;

	perceptual_roughness = clamp(perceptual_roughness, c_min_roughness, 1.0);
	metallic = clamp(metallic, 0.0, 1.0);
	// Roughness is authored as perceptual roughness; as is convention,
	// convert to material roughness by squaring the perceptual roughness [2].
	float alpha_roughness = perceptual_roughness * perceptual_roughness;

	// The albedo may be defined from a base texture or a flat color
	vec4 base_color = albedo;

	vec3 f0 = vec3(0.04);
	vec3 diffuse_color = base_color.rgb * (vec3(1.0) - f0);
	diffuse_color *= 1.0 - metallic;
	vec3 specular_color = mix(f0, base_color.rgb, metallic);

	// Compute reflectance.
	float reflectance = max(max(specular_color.r, specular_color.g), specular_color.b);

	// For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
	// For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
	float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
	vec3 specular_environment_r0 = specular_color.rgb;
	vec3 specular_environment_r90 = vec3(1.0, 1.0, 1.0) * reflectance90;

	vec3 n = normalize(normal);					// normal at surface point
	vec3 v = normalize(camera_position_world - position_world);	// Vector from surface point to camera
	vec3 reflection = -normalize(reflect(v, n));

	pbr_inputs.n_dot_v = clamp(abs(dot(n, v)), 0.001, 1.0);
	pbr_inputs.perceptual_roughness = perceptual_roughness;
	pbr_inputs.reflectance0 = specular_environment_r0;
	pbr_inputs.reflectance90 = specular_environment_r90;
	pbr_inputs.alpha_roughness = alpha_roughness;
	pbr_inputs.diffuse_color = diffuse_color;
	pbr_inputs.specular_color = specular_color;
	pbr_inputs.normal = n;
	pbr_inputs.vec_from_surface_to_camera = v;

	// Calculate lighting contribution from image based lighting source (IBL)
	vec3 color = GetIblContribution(pbr_inputs, n, reflection, tex_env_map, tex_env_map_irradiance, tex_brdf_lut);

	return color;
}

vec3 CalculatePBRLightContribution(inout PbrInfo pbr_inputs, vec3 light_direction, vec3 light_color)
{
	vec3 n = pbr_inputs.normal;
	vec3 v = pbr_inputs.vec_from_surface_to_camera;
	vec3 l = normalize(light_direction);	// Vector from surface point to light
	vec3 h = normalize(l + v);				// Half vector between both l and v

	float n_dot_v = pbr_inputs.n_dot_v;
	float n_dot_l = clamp(dot(n, l), 0.001, 1.0);
	float n_dot_h = clamp(dot(n, h), 0.0, 1.0);
	float l_dot_h = clamp(dot(l, h), 0.0, 1.0);
	float v_dot_h = clamp(dot(v, h), 0.0, 1.0);

	pbr_inputs.n_dot_l = n_dot_l;
	pbr_inputs.n_dot_h = n_dot_h;
	pbr_inputs.l_dot_h = l_dot_h;
	pbr_inputs.v_dot_h = v_dot_h;

	// Calculate the shading terms for the microfacet specular shading model
	vec3 f = SpecularReflection(pbr_inputs);
	float g = GeometricOcclusion(pbr_inputs);
	float d = MicrofacetDistribution(pbr_inputs);

	// Calculation of analytical lighting contribution
	vec3 diffuse_contrib = (1.0 - f) * DiffuseBurley(pbr_inputs);
	vec3 spec_contrib = f * g * d / (4.0 * n_dot_l * n_dot_v);
	// Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
	vec3 color = n_dot_l * light_color * (diffuse_contrib + spec_contrib);

	return color;
}

/**
 * Calculates a transform that will convert a normal from the normal map space to the world space
 * oriented according to the normal of the surface.
 * @param normal The surface normal in world space. This is the geometry normal, not the normal from the normal map.
 * @param position A vector from the pixel on the surface to the camera in world space.
 * @param uv The texture coordinates of the pixel.
 * @return A matrix that will convert a normal from the normal map space to the world space.
 * @note Explanation for the algorithm can be found at: http://www.thetenthplanet.de/archives/1180
 */
mat3 CotangentFrame(vec3 normal, vec3 position, vec2 uv)
{
	// get edge vectors of the pixel triangle
	vec3 dp1 = dFdx(position);
	vec3 dp2 = dFdy(position);
	vec2 duv1 = dFdx(uv);
	vec2 duv2 = dFdy(uv);

	// solve the linear system
	vec3 dp2perp = cross(dp2, normal);
	vec3 dp1perp = cross(normal, dp1);
	vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
	vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

	// construct a scale-invariant frame
	float invmax = inversesqrt(max(dot(T,T), dot(B,B)));

	// calculate handedness of the resulting cotangent frame
	float w = (dot(cross(normal, T), B) < 0.0) ? -1.0 : 1.0;

	// adjust tangent if needed
	T = T * w;

	return mat3(T * invmax, B * invmax, normal);
}

/**
 * Perturbs a normal using a normal map. This means that normal from the normal map is converted
 * from the normal map space to the world space.
 * @param n The surface normal in world space. This is the geometry normal, not the normal from the normal map.
 * @param v A vector from the pixel on the surface to the camera in world space.
 * @param normal_sample The normal from the normal map in the normal map space.
 * @param uv The texture coordinates of the pixel.
 * @return The perturbed normal in world space.
 */
vec3 PerturbNormal(vec3 n, vec3 v, vec3 normal_sample, vec2 uv)
{
	vec3 map = normalize(2.0 * normal_sample - vec3(1.0));
	mat3 tbn = CotangentFrame(n, v, uv);
	return normalize(tbn * map);
}
