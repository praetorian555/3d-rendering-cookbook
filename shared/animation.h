#pragma once

#include "opal/container/dynamic-array.h"
#include "opal/container/in-place-array.h"
#include "opal/container/string.h"

#include "rndr/definitions.h"
#include "rndr/math.h"

#include "types.h"

struct Bone
{
    Opal::StringUtf8 name;
    i32 id;
    Rndr::Matrix4x4f inverse_bind_transform;
};

struct SkeletalMeshDescription
{
    static constexpr u32 k_max_lods = 8;
    static constexpr u32 k_max_streams = 8;

    /** Total size of the mesh data in bytes. Equal to sum of all vertices and all indices. */
    size_t mesh_size = 0;

    /** Number of vertices belonging to this mesh in the vertex buffer. */
    i64 vertex_count = 0;

    /** Offset of the mesh in the vertex buffer in vertices. */
    i64 vertex_offset = 0;

    /** Sizes of the vertex in bytes. */
    size_t vertex_size = 0;

    /** Offset of the mesh in the index buffer in indices. */
    i64 index_offset = 0;

    /** Number of LODs of this mesh. */
    i64 lod_count = 0;

    /** Offsets of the LODs in indices starting from 0. First index is reserved for most detailed version of the mesh. */
    Opal::InPlaceArray<u32, k_max_lods> lod_offsets = {};

    [[nodiscard]] RNDR_FORCE_INLINE i64 GetLodIndicesCount(i64 lod) const
    {
        RNDR_ASSERT(lod < lod_count, "LOD index out of range");
        return lod_offsets[lod + 1] - lod_offsets[lod];
    }
};

/**
 * Collection of one or more skeletal meshes in a single vertex and index buffers. It also contains descriptions of all meshes.
 */
struct SkeletalMeshData
{
    /** Descriptions of all meshes. */
    Opal::DynamicArray<SkeletalMeshDescription> meshes;
    /** Vertex buffer data. */
    Opal::DynamicArray<u8> vertex_buffer_data;
    /** Index buffer data. */
    Opal::DynamicArray<u8> index_buffer_data;
};