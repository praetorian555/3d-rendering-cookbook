//
#version 460 core

struct Vertex
{
    float p[3];
    float n[3];
    float tc[2];
};

layout(std430, binding = 1) restrict readonly buffer Vertices
{
    Vertex in_vertices[];
};

layout(std430, binding = 2) restrict readonly buffer Matrices
{
    mat4 in_model_matrices[];
};

layout(std140, binding = 0) uniform PerFrameData
{
    mat4 clip_from_world;
    mat4 light_clip_from_world;
    vec4 world_camera_position;
    vec4 light_angles;
    vec4 world_light_position;
};

vec3 GetPosition(int i)
{
    return vec3(in_vertices[i].p[0], in_vertices[i].p[1], in_vertices[i].p[2]);
}

vec2 GetTexCoord(int i)
{
    return vec2(in_vertices[i].tc[0], in_vertices[i].tc[1]);
}

struct PerVertex
{
    vec2 uv;
    vec4 shadow_coord;
    vec3 vertex_world_position;
};

layout (location=0) out PerVertex vtx;

// OpenGL's Z is in -1..1
const mat4 scale_bias = mat4(
0.5, 0.0, 0.0, 0.0,
0.0, 0.5, 0.0, 0.0,
0.0, 0.0, 0.5, 0.0,
0.5, 0.5, 0.5, 1.0 );

void main()
{
    mat4 world_from_model = in_model_matrices[gl_BaseInstance];
    mat4 clip_from_model = clip_from_world * world_from_model;

    vec3 model_position = GetPosition(gl_VertexID);

    gl_Position = clip_from_model * vec4(model_position, 1.0);

    vtx.uv = GetTexCoord(gl_VertexID);
    // Since we need to use shadow_coord to sample a shadow map, we need to scale it to the range [0, 1] which means
    // that we need to scale it by 0.5 and add 0.5.
    vtx.shadow_coord = scale_bias * light_clip_from_world * world_from_model * vec4(model_position, 1.0);
    vtx.vertex_world_position = (world_from_model * vec4(model_position, 1.0)).xyz;
}
