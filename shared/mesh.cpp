#include "mesh.h"

#include "rndr/file.h"
#include "rndr/log.h"

namespace
{
constexpr uint32_t k_magic = 0x89ABCDEF;
}

bool Mesh::ReadData(MeshData& out_mesh_data, const Opal::StringUtf8& file_path)
{
    const char8* file_path_raw = file_path.GetData();
    Rndr::FileHandler f(file_path_raw, "rb");
    if (!f.IsValid())
    {
        RNDR_LOG_ERROR("Failed to open file %s!", file_path.GetData());
        return false;
    }

    MeshFileHeader header;
    if (!f.Read(&header, sizeof(MeshFileHeader), 1))
    {
        RNDR_LOG_ERROR("Failed to read mesh file header!");
        return false;
    }
    if (header.magic != k_magic)
    {
        RNDR_LOG_ERROR("Invalid mesh file magic!");
        return false;
    }

    if (header.mesh_count > 0)
    {
        out_mesh_data.meshes.Resize(header.mesh_count);
        if (!f.Read(out_mesh_data.meshes.GetData(), sizeof(out_mesh_data.meshes[0]), header.mesh_count))
        {
            RNDR_LOG_ERROR("Failed to read mesh descriptions!");
            return false;
        }
    }

    if (header.vertex_buffer_size > 0)
    {
        out_mesh_data.vertex_buffer_data.Resize(header.vertex_buffer_size);
        if (!f.Read(out_mesh_data.vertex_buffer_data.GetData(), sizeof(out_mesh_data.vertex_buffer_data[0]), header.vertex_buffer_size))
        {
            RNDR_LOG_ERROR("Failed to read vertex buffer data!");
            return false;
        }
    }

    if (header.index_buffer_size > 0)
    {
        out_mesh_data.index_buffer_data.Resize(header.index_buffer_size);
        if (!f.Read(out_mesh_data.index_buffer_data.GetData(), sizeof(out_mesh_data.index_buffer_data[0]), header.index_buffer_size))
        {
            RNDR_LOG_ERROR("Failed to read index buffer data!");
            return false;
        }
    }

    if (header.mesh_count > 0)
    {
        out_mesh_data.bounding_boxes.Resize(header.mesh_count);
        if (!f.Read(out_mesh_data.bounding_boxes.GetData(), sizeof(out_mesh_data.bounding_boxes[0]), header.mesh_count))
        {
            RNDR_LOG_ERROR("Failed to read bounding boxes!");
            return false;
        }
    }

    return true;
}

bool Mesh::WriteData(const MeshData& mesh_data, const Opal::StringUtf8& file_path)
{
    const char8* file_path_raw = file_path.GetData();
    Rndr::FileHandler f(file_path_raw, "wb");
    if (!f.IsValid())
    {
        RNDR_LOG_ERROR("Failed to open file %s!", file_path.GetData());
        return false;
    }

    MeshFileHeader header;
    header.magic = k_magic;
    header.version = 1;
    header.mesh_count = static_cast<int64_t>(mesh_data.meshes.GetSize());
    header.data_offset = static_cast<int64_t>(header.mesh_count * sizeof(MeshDescription) + sizeof(header));
    header.vertex_buffer_size = mesh_data.vertex_buffer_data.GetSize();
    header.index_buffer_size = mesh_data.index_buffer_data.GetSize();

    f.Write(&header, sizeof(header), 1);
    if (header.mesh_count > 0)
    {
        f.Write(mesh_data.meshes.GetData(), sizeof(mesh_data.meshes[0]), mesh_data.meshes.GetSize());
    }
    if (header.vertex_buffer_size > 0)
    {
        f.Write(mesh_data.vertex_buffer_data.GetData(), sizeof(mesh_data.vertex_buffer_data[0]), mesh_data.vertex_buffer_data.GetSize());
    }
    if (header.index_buffer_size > 0)
    {
        f.Write(mesh_data.index_buffer_data.GetData(), sizeof(mesh_data.index_buffer_data[0]), mesh_data.index_buffer_data.GetSize());
    }
    if (header.mesh_count > 0)
    {
        f.Write(mesh_data.bounding_boxes.GetData(), sizeof(mesh_data.bounding_boxes[0]), mesh_data.bounding_boxes.GetSize());
    }

    return true;
}

bool Mesh::UpdateBoundingBoxes(MeshData& mesh_data)
{
    mesh_data.bounding_boxes.Clear();
    mesh_data.bounding_boxes.Resize(mesh_data.meshes.GetSize());

    for (size_t i = 0; i < mesh_data.meshes.GetSize(); ++i)
    {
        const MeshDescription& mesh_desc = mesh_data.meshes[i];
        const int64_t index_count = mesh_desc.GetLodIndicesCount(0);

        Point3f min(Opal::k_largest_float);
        Point3f max(Opal::k_smallest_float);

        uint32_t* index_buffer = reinterpret_cast<uint32_t*>(mesh_data.index_buffer_data.GetData());
        float* vertex_buffer = reinterpret_cast<float*>(mesh_data.vertex_buffer_data.GetData());
        for (int64_t j = 0; j < index_count; ++j)
        {
            const int64_t vertex_offset = mesh_desc.vertex_offset + index_buffer[mesh_desc.index_offset + j];
            const float* vertex = vertex_buffer + vertex_offset * (mesh_desc.vertex_size / sizeof(float));
            min = Opal::Min(min, Point3f(vertex[0], vertex[1], vertex[2]));
            max = Opal::Max(max, Point3f(vertex[0], vertex[1], vertex[2]));
        }

        mesh_data.bounding_boxes[i] = Bounds3f(min, max);
    }

    return false;
}

bool Mesh::Merge(MeshData& out_mesh_data, const Opal::ArrayView<MeshData>& mesh_data)
{
    if (mesh_data.IsEmpty())
    {
        return false;
    }

    int64_t vertex_offset = 0;
    int64_t index_offset = 0;
    for (const MeshData& mesh : mesh_data)
    {
        for (const MeshDescription& mesh_desc : mesh.meshes)
        {
            MeshDescription new_mesh_desc = mesh_desc;
            new_mesh_desc.vertex_offset += vertex_offset;
            new_mesh_desc.index_offset += index_offset;
            out_mesh_data.meshes.PushBack(new_mesh_desc);

            vertex_offset += mesh_desc.vertex_count;
            for (int64_t i = 0; i < mesh_desc.lod_count; ++i)
            {
                index_offset += mesh_desc.GetLodIndicesCount(i);
            }
        }

        out_mesh_data.vertex_buffer_data.Insert(out_mesh_data.vertex_buffer_data.cend(), mesh.vertex_buffer_data.cbegin(),
                                                mesh.vertex_buffer_data.cend());
        out_mesh_data.index_buffer_data.Insert(out_mesh_data.index_buffer_data.cend(), mesh.index_buffer_data.cbegin(),
                                               mesh.index_buffer_data.cend());
    }

    UpdateBoundingBoxes(out_mesh_data);

    return true;
}

bool Mesh::GetDrawCommands(Opal::DynamicArray<Rndr::DrawIndicesData>& out_draw_commands, const Opal::DynamicArray<MeshDrawData>& mesh_draw_data,
                           const MeshData& mesh_data)
{
    out_draw_commands.Resize(mesh_draw_data.GetSize());
    for (int i = 0; i < out_draw_commands.GetSize(); i++)
    {
        const int64_t mesh_idx = mesh_draw_data[i].mesh_index;
        const int64_t lod = mesh_draw_data[i].lod;
        const MeshDescription& mesh_desc = mesh_data.meshes[mesh_idx];
        const int64_t index_count = mesh_desc.GetLodIndicesCount(lod);
        RNDR_ASSERT(index_count >= 0 && index_count <= static_cast<int64_t>(UINT32_MAX));
        RNDR_ASSERT(mesh_draw_data[i].index_buffer_offset >= 0 &&
                    mesh_draw_data[i].index_buffer_offset <= static_cast<int64_t>(UINT32_MAX));
        RNDR_ASSERT(mesh_draw_data[i].vertex_buffer_offset >= 0 &&
                    mesh_draw_data[i].vertex_buffer_offset <= static_cast<int64_t>(UINT32_MAX));
        RNDR_ASSERT(mesh_draw_data[i].material_index >= 0 && mesh_draw_data[i].material_index <= static_cast<int64_t>(UINT32_MAX));
        out_draw_commands[i] = {.index_count = static_cast<uint32_t>(index_count),
                                .instance_count = 1,
                                .first_index = static_cast<uint32_t>(mesh_draw_data[i].index_buffer_offset),
                                .base_vertex = static_cast<uint32_t>(mesh_draw_data[i].vertex_buffer_offset),
                                .base_instance = static_cast<uint32_t>(mesh_draw_data[i].material_index)};
    }
    return true;
}

Rndr::ErrorCode Mesh::AddPlaneXZ(MeshData& out_mesh_data, const Point3f& center, f32 scale, MeshAttributesToLoad attributes_to_load)
{
    const Opal::InPlaceArray<Rndr::Point3f, 4> vertices = {
        Rndr::Point3f(center.x - scale, center.y, center.z - scale),
        Rndr::Point3f(center.x - scale, center.y, center.z + scale),
        Rndr::Point3f(center.x + scale, center.y, center.z + scale),
        Rndr::Point3f(center.x + scale, center.y, center.z - scale),
    };

    const Opal::InPlaceArray<Rndr::Vector3f, 4> normals = {
        Rndr::Vector3f(0, 0, 1),
        Rndr::Vector3f(0, 0, 1),
        Rndr::Vector3f(0, 0, 1),
        Rndr::Vector3f(0, 0, 1),
    };

    const Opal::InPlaceArray<Rndr::Point2f, 4> uvs = {
        Rndr::Point2f(0, 0),
        Rndr::Point2f(0, 1),
        Rndr::Point2f(1, 1),
        Rndr::Point2f(1, 0),
    };

    MeshDescription mesh_desc;
    mesh_desc.vertex_size = sizeof(Rndr::Point3f);

    if (!!(attributes_to_load & MeshAttributesToLoad::LoadNormals))
    {
        mesh_desc.vertex_size += sizeof(Rndr::Vector3f);
    }

    if (!!(attributes_to_load & MeshAttributesToLoad::LoadUvs))
    {
        mesh_desc.vertex_size += sizeof(Rndr::Point2f);
    }

    mesh_desc.vertex_offset = static_cast<int64_t>(out_mesh_data.vertex_buffer_data.GetSize() / mesh_desc.vertex_size);
    mesh_desc.index_offset = static_cast<int64_t>(out_mesh_data.index_buffer_data.GetSize() / sizeof(u32));
    mesh_desc.vertex_count = 4;
    mesh_desc.lod_count = 1;
    mesh_desc.lod_offsets[0] = 0;
    mesh_desc.lod_offsets[1] = 6;
    mesh_desc.mesh_size = mesh_desc.vertex_count * mesh_desc.vertex_size + mesh_desc.lod_offsets[1] * sizeof(u32);
    out_mesh_data.meshes.PushBack(mesh_desc);

    for (i32 i = 0; i < 4; i++)
    {
        const u8* vertex_data = reinterpret_cast<const u8*>(vertices[i].data);
        out_mesh_data.vertex_buffer_data.Insert(out_mesh_data.vertex_buffer_data.cend(), vertex_data,
                                                vertex_data + sizeof(Rndr::Point3f));

        if (!!(attributes_to_load & MeshAttributesToLoad::LoadNormals))
        {
            const u8* normal_data = reinterpret_cast<const u8*>(normals[i].data);
            out_mesh_data.vertex_buffer_data.Insert(out_mesh_data.vertex_buffer_data.cend(), normal_data,
                                                    normal_data + sizeof(Rndr::Vector3f));
        }

        if (!!(attributes_to_load & MeshAttributesToLoad::LoadUvs))
        {
            const u8* uv_data = reinterpret_cast<const u8*>(uvs[i].data);
            out_mesh_data.vertex_buffer_data.Insert(out_mesh_data.vertex_buffer_data.cend(), uv_data, uv_data + sizeof(Rndr::Point2f));
        }
    }

    u32 vertex_base = static_cast<u32>(mesh_desc.vertex_offset);
    const u32 indices[] = {vertex_base + 0, vertex_base + 1, vertex_base + 2, vertex_base + 2, vertex_base + 3, vertex_base + 0};
    for (u32 i = 0; i < 6; i++)
    {
        out_mesh_data.index_buffer_data.Insert(out_mesh_data.index_buffer_data.cend(), reinterpret_cast<const u8*>(&indices[i]),
                                               reinterpret_cast<const u8*>(&indices[i]) + sizeof(indices[i]));
    }

    return Rndr::ErrorCode::Success;
}
