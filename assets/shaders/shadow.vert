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
    mat4 projection_view;
};

vec3 GetPosition(int i)
{
    return vec3(in_vertices[i].p[0], in_vertices[i].p[1], in_vertices[i].p[2]);
}

layout (location=0) out vec4 out_model_position;

void main()
{
    mat4 mvp = projection_view * in_model_matrices[0];
    out_model_position = vec4(GetPosition(gl_VertexID), 1.0);
    gl_Position = mvp * vec4(GetPosition(gl_VertexID), 1.0);
}
