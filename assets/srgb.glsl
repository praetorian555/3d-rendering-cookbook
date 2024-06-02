vec4 SrgbToLinear(vec4 in_srgb, float gamma)
{
    vec3 out_lin = pow(in_srgb.xyz,vec3(gamma));
    return vec4(out_lin, in_srgb.a);
}

vec4 LinearToSrgb(vec4 in_lin, float gamma)
{
    vec3 out_srgb = pow(in_lin.xyz,vec3(1.0/gamma));
    return vec4(out_srgb, in_lin.a);
}