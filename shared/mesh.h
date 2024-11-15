#pragma once

#include "opal/container/dynamic-array.h"
#include "opal/container/array-view.h"
#include "opal/container/in-place-array.h"
#include "opal/container/string.h"

#include "rndr/enum-flags.h"
#include "rndr/error-codes.h"
#include "rndr/graphics-types.h"

#include "types.h"

struct aiScene;

/**
 * Description of a single mesh. It contains information about the mesh's streams and LODs. It does not contain the actual
 * mesh data. Mesh data is stored in MeshData.
 */
struct MeshDescription final
{
public:
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
        RNDR_ASSERT(lod < lod_count);
        return lod_offsets[lod + 1] - lod_offsets[lod];
    }
};

/**
 * Collection of multiple meshes all stored in single vertex and index buffers. It also contains descriptions of all meshes.
 */
struct MeshData
{
    /** Descriptions of all meshes. */
    Opal::DynamicArray<MeshDescription> meshes;
    /** Vertex buffer data. */
    Opal::DynamicArray<u8> vertex_buffer_data;
    /** Index buffer data. */
    Opal::DynamicArray<u8> index_buffer_data;
    /** Bounding boxes of all meshes. */
    Opal::DynamicArray<Bounds3f> bounding_boxes;
};

/**
 * Used to create indirect draw commands for rendering meshes.
 */
struct MeshDrawData
{
    /** Mesh index in the meshes array in MeshData. */
    i64 mesh_index;
    /** Material index in the materials array in SceneDrawData. */
    i64 material_index;
    /** LOD index in the MeshDescription. */
    i64 lod;
    /** Offset in vertex buffer in vertices. */
    i64 vertex_buffer_offset;
    /** Offset in index buffer in indices. */
    i64 index_buffer_offset;
    /** Transform index in the SceneDescription. */
    i64 transform_index;
};

/**
 * Header of the mesh file.
 */
struct MeshFileHeader
{
    /** Magic number identifying the file format. */
    u32 magic;
    /** Version of the file format. */
    u32 version;
    /** Number of meshes in the file. */
    i64 mesh_count;
    /** Offset of the mesh data in the file. */
    i64 data_offset;
    /** Size of vertex data in the file. */
    size_t vertex_buffer_size;
    /** Size of index data in the file. */
    size_t index_buffer_size;
};

enum class MeshAttributesToLoad : u8
{
    LoadPositions = 1 << 0,
    LoadNormals = 1 << 1,
    LoadUvs = 1 << 2,
    LoadAll = LoadPositions | LoadNormals | LoadUvs,
};
RNDR_ENUM_CLASS_FLAGS(MeshAttributesToLoad)

namespace Mesh
{

/**
 * Reads mesh data from a file containing optimized rndr mesh data format.
 * @param out_mesh_data Destination mesh data.
 * @param file_path Path to the file.
 * @return True if mesh data was read successfully, false otherwise.
 */
bool ReadData(MeshData& out_mesh_data, const Opal::StringUtf8& file_path);

/**
 * Writes mesh data to a file containing optimized rndr mesh data format.
 * @param mesh_data Mesh data to write.
 * @param file_path Path to the file.
 * @return True if mesh data was written successfully, false otherwise.
 */
bool WriteData(const MeshData& mesh_data, const Opal::StringUtf8& file_path);

/**
 * Updates bounding boxes of all meshes in the mesh data.
 * @param mesh_data Mesh data to update.
 * @return True if bounding boxes were updated successfully, false otherwise.
 */
bool UpdateBoundingBoxes(MeshData& mesh_data);

/**
 * Merges multiple mesh data into single mesh data. All meshes are stored in single vertex and index buffers. Mesh descriptions are updated
 * accordingly.
 * @param out_mesh_data Destination mesh data.
 * @param mesh_data Mesh data to merge.
 * @return True if mesh data was merged successfully, false otherwise.
 */
bool Merge(MeshData& out_mesh_data, const Opal::ArrayView<MeshData>& mesh_data);

/**
 * Create draw commands that can be used with DrawIndicesMulti API to render meshes.
 * @param out_draw_commands Destination draw commands.
 * @param mesh_draw_data Draw data for all meshes.
 * @param mesh_data Mesh graphics API buffer data.
 * @return True if draw commands were created successfully, false otherwise.
 * @note base_instance field in the DrawIndicesData will store material index. Instance count will be set to 1.
 */
bool GetDrawCommands(Opal::DynamicArray<Rndr::DrawIndicesData>& out_draw_commands, const Opal::DynamicArray<MeshDrawData>& mesh_draw_data,
                     const MeshData& mesh_data);

Rndr::ErrorCode AddPlaneXZ(MeshData& out_mesh_data, const Rndr::Point3f& center, f32 scale, MeshAttributesToLoad attributes_to_load);

}  // namespace Mesh
