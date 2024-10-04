#include "assimp-helpers.h"

#include <stack>

#include <assimp/cimport.h>
#include <assimp/material.h>
#include <assimp/pbrmaterial.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "rndr/log.h"

#include "material.h"
#include "scene.h"
#include "types.h"

namespace
{
void Traverse(SceneDescription& out_scene, const aiScene* ai_scene, const aiNode* ai_node, Scene::NodeId parent, int32_t level);
}

Matrix4x4f AssimpHelpers::Convert(const aiMatrix4x4& ai_matrix)
{
    return {ai_matrix.a1, ai_matrix.a2, ai_matrix.a3, ai_matrix.a4, ai_matrix.b1, ai_matrix.b2, ai_matrix.b3, ai_matrix.b4,
            ai_matrix.c1, ai_matrix.c2, ai_matrix.c3, ai_matrix.c4, ai_matrix.d1, ai_matrix.d2, ai_matrix.d3, ai_matrix.d4};
}

bool AssimpHelpers::ReadMeshData(MeshData& out_mesh_data, const aiScene& ai_scene, MeshAttributesToLoad attributes_to_load)
{
    if (!ai_scene.HasMeshes())
    {
        RNDR_LOG_ERROR("No meshes in the assimp scene!");
        return false;
    }

    const bool should_load_normals = !!(attributes_to_load & MeshAttributesToLoad::LoadNormals);
    const bool should_load_uvs = !!(attributes_to_load & MeshAttributesToLoad::LoadUvs);
    u32 vertex_size = sizeof(Rndr::Point3f);
    if (should_load_normals)
    {
        vertex_size += sizeof(Rndr::Normal3f);
    }
    if (should_load_uvs)
    {
        vertex_size += sizeof(Rndr::Point2f);
    }

    u32 vertex_offset = 0;
    u32 index_offset = 0;
    for (u32 mesh_index = 0; mesh_index < ai_scene.mNumMeshes; ++mesh_index)
    {
        const aiMesh* const ai_mesh = ai_scene.mMeshes[mesh_index];

        for (u32 i = 0; i < ai_mesh->mNumVertices; ++i)
        {
            Rndr::Point3f position(ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z);
            out_mesh_data.vertex_buffer_data.Insert(out_mesh_data.vertex_buffer_data.ConstEnd(), reinterpret_cast<u8*>(position.data),
                                                    reinterpret_cast<u8*>(position.data) + sizeof(position));

            if (should_load_normals)
            {
                RNDR_ASSERT(ai_mesh->HasNormals());
                Rndr::Normal3f normal(ai_mesh->mNormals[i].x, ai_mesh->mNormals[i].y, ai_mesh->mNormals[i].z);
                out_mesh_data.vertex_buffer_data.Insert(out_mesh_data.vertex_buffer_data.ConstEnd(),
                                                        reinterpret_cast<uint8_t*>(normal.data),
                                                        reinterpret_cast<uint8_t*>(normal.data) + sizeof(normal));
            }
            if (should_load_uvs)
            {
                aiVector3D ai_uv = ai_mesh->HasTextureCoords(0) ? ai_mesh->mTextureCoords[0][i] : aiVector3D();
                Rndr::Point2f uv(ai_uv.x, ai_uv.y);
                out_mesh_data.vertex_buffer_data.Insert(out_mesh_data.vertex_buffer_data.ConstEnd(), reinterpret_cast<uint8_t*>(uv.data),
                                                        reinterpret_cast<uint8_t*>(uv.data) + sizeof(uv));
            }
        }

        Opal::Array<Opal::Array<u32>> lods(MeshDescription::k_max_lods);
        for (u32 i = 0; i < ai_mesh->mNumFaces; ++i)
        {
            const aiFace& face = ai_mesh->mFaces[i];
            if (face.mNumIndices != 3)
            {
                continue;
            }
            for (u32 j = 0; j < face.mNumIndices; ++j)
            {
                lods[0].PushBack(face.mIndices[j]);
            }
        }

        out_mesh_data.index_buffer_data.Insert(out_mesh_data.index_buffer_data.ConstEnd(), reinterpret_cast<uint8_t*>(lods[0].GetData()),
                                               reinterpret_cast<uint8_t*>(lods[0].GetData()) + lods[0].GetSize() * sizeof(u32));

        // TODO: Generate LODs

        MeshDescription mesh_desc;
        mesh_desc.vertex_count = ai_mesh->mNumVertices;
        mesh_desc.vertex_offset = vertex_offset;
        mesh_desc.vertex_size = vertex_size;
        mesh_desc.index_offset = index_offset;
        mesh_desc.lod_count = 1;
        mesh_desc.lod_offsets[0] = 0;
        mesh_desc.lod_offsets[1] = static_cast<u32>(lods[0].GetSize());
        mesh_desc.mesh_size = ai_mesh->mNumVertices * vertex_size + static_cast<u32>(lods[0].GetSize()) * sizeof(u32);

        // TODO: Add material info

        out_mesh_data.meshes.PushBack(mesh_desc);

        vertex_offset += ai_mesh->mNumVertices;
        index_offset += static_cast<u32>(lods[0].GetSize());
    }

    Mesh::UpdateBoundingBoxes(out_mesh_data);

    return true;
}

bool AssimpHelpers::ReadMeshData(MeshData& out_mesh_data, const Opal::StringUtf8& mesh_file_path, MeshAttributesToLoad attributes_to_load)
{
    constexpr u32 k_ai_process_flags = aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                                       aiProcess_LimitBoneWeights | aiProcess_SplitLargeMeshes | aiProcess_ImproveCacheLocality |
                                       aiProcess_RemoveRedundantMaterials | aiProcess_FindDegenerates | aiProcess_FindInvalidData |
                                       aiProcess_GenUVCoords;

    const aiScene* scene = aiImportFile(mesh_file_path.GetDataAs<c>(), k_ai_process_flags);
    if (scene == nullptr || !scene->HasMeshes())
    {
        RNDR_LOG_ERROR("Failed to load mesh from file with error: %s", aiGetErrorString());
        return false;
    }

    if (!ReadMeshData(out_mesh_data, *scene, attributes_to_load))
    {
        RNDR_LOG_ERROR("Failed to load mesh data from file: %s", mesh_file_path.GetDataAs<c>());
        return false;
    }

    aiReleaseImport(scene);

    return true;
}

namespace
{
ImageId AddUnique(Opal::Array<Opal::StringUtf8>& files, const char* path)
{
    const Opal::StringUtf8 path_utf8(reinterpret_cast<const c8*>(path));
    for (u32 i = 0; i < files.GetSize(); ++i)
    {
        if (files[i] == path_utf8)
        {
            return i;
        }
    }
    files.PushBack(path_utf8);
    return files.GetSize() - 1;
}
}  // namespace

bool AssimpHelpers::ReadMaterialDescription(MaterialDescription& out_description, Opal::Array<Opal::StringUtf8>& out_texture_paths,
                                            Opal::Array<Opal::StringUtf8>& out_opacity_maps, const aiMaterial& ai_material)
{
    aiColor4D ai_color;
    if (aiGetMaterialColor(&ai_material, AI_MATKEY_COLOR_AMBIENT, &ai_color) == AI_SUCCESS)
    {
        out_description.emissive_color = Vector4f(ai_color.r, ai_color.g, ai_color.b, ai_color.a);
        out_description.emissive_color.a = Math::Clamp(out_description.emissive_color.a, 0.0f, 1.0f);
    }
    if (aiGetMaterialColor(&ai_material, AI_MATKEY_COLOR_EMISSIVE, &ai_color) == AI_SUCCESS)
    {
        out_description.emissive_color.r += ai_color.r;
        out_description.emissive_color.g += ai_color.g;
        out_description.emissive_color.b += ai_color.b;
        out_description.emissive_color.a += ai_color.a;
        out_description.emissive_color.a = Math::Clamp(out_description.emissive_color.a, 0.0f, 1.0f);
    }
    if (aiGetMaterialColor(&ai_material, AI_MATKEY_COLOR_DIFFUSE, &ai_color) == AI_SUCCESS)
    {
        out_description.albedo_color = Vector4f(ai_color.r, ai_color.g, ai_color.b, ai_color.a);
        out_description.albedo_color.a = Math::Clamp(out_description.albedo_color.a, 0.0f, 1.0f);
    }

    // Read opacity factor from the AI material and convert it to transparency factor. If opacity is 95% or more, the material is considered
    // opaque.
    constexpr float k_opaqueness_threshold = 0.05f;
    float opacity = 1.0f;
    if (aiGetMaterialFloat(&ai_material, AI_MATKEY_OPACITY, &opacity) == AI_SUCCESS)
    {
        out_description.transparency_factor = 1.0f - opacity;
        out_description.transparency_factor = Math::Clamp(out_description.transparency_factor, 0.0f, 1.0f);
        if (out_description.transparency_factor >= 1.0f - k_opaqueness_threshold)
        {
            out_description.transparency_factor = 0.0f;
        }
    }

    // If AI material contains transparency factor as an RGB value, it will take precedence over the opacity factor.
    if (aiGetMaterialColor(&ai_material, AI_MATKEY_COLOR_TRANSPARENT, &ai_color) == AI_SUCCESS)
    {
        opacity = Math::Max(Math::Max(ai_color.r, ai_color.g), ai_color.b);
        out_description.transparency_factor = Math::Clamp(opacity, 0.0f, 1.0f);
        if (out_description.transparency_factor >= 1.0f - k_opaqueness_threshold)
        {
            out_description.transparency_factor = 0.0f;
        }
        out_description.alpha_test = 0.5f;
    }

    // Read roughness and metallic factors from the AI material.
    float factor = 1.0f;
    if (aiGetMaterialFloat(&ai_material, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, &factor) == AI_SUCCESS)
    {
        out_description.metallic_factor = factor;
    }
    if (aiGetMaterialFloat(&ai_material, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, &factor) == AI_SUCCESS)
    {
        out_description.roughness = Vector4f(factor, factor, 0.0f, 0.0f);
    }

    // Get info about the texture file paths, store them in the out_texture_paths array and set the corresponding image ids in the material
    // description.
    aiString out_texture_path;
    aiTextureMapping out_texture_mapping = aiTextureMapping_UV;
    unsigned int out_uv_index = 0;
    float out_blend = 1.0f;
    aiTextureOp out_texture_op = aiTextureOp_Add;
    Opal::StackArray<aiTextureMapMode, 2> out_texture_mode = {aiTextureMapMode_Wrap, aiTextureMapMode_Wrap};
    unsigned int out_texture_flags = 0;
    if (aiGetMaterialTexture(&ai_material, aiTextureType_EMISSIVE, 0, &out_texture_path, &out_texture_mapping, &out_uv_index, &out_blend,
                             &out_texture_op, out_texture_mode.data(), &out_texture_flags) == AI_SUCCESS)
    {
        out_description.emissive_texture = AddUnique(out_texture_paths, out_texture_path.C_Str());
    }
    if (aiGetMaterialTexture(&ai_material, aiTextureType_DIFFUSE, 0, &out_texture_path, &out_texture_mapping, &out_uv_index, &out_blend,
                             &out_texture_op, out_texture_mode.data(), &out_texture_flags) == AI_SUCCESS)
    {
        out_description.albedo_texture = AddUnique(out_texture_paths, out_texture_path.C_Str());
        // Some material heuristics
        const Opal::StringUtf8 albedo_map_path(reinterpret_cast<const c8*>(out_texture_path.C_Str()));
        if (Opal::Find(albedo_map_path, OPAL_UTF8("grey_30")) != Opal::StringUtf8::k_npos)
        {
            out_description.flags |= MaterialFlags::Transparent;
        }
    }
    if (aiGetMaterialTexture(&ai_material, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &out_texture_path,
                             &out_texture_mapping, &out_uv_index, &out_blend, &out_texture_op, out_texture_mode.data(),
                             &out_texture_flags) == AI_SUCCESS)
    {
        out_description.metallic_roughness_texture = AddUnique(out_texture_paths, out_texture_path.C_Str());
    }
    if (aiGetMaterialTexture(&ai_material, aiTextureType_LIGHTMAP, 0, &out_texture_path, &out_texture_mapping, &out_uv_index, &out_blend,
                             &out_texture_op, out_texture_mode.data(), &out_texture_flags) == AI_SUCCESS)
    {
        out_description.ambient_occlusion_texture = AddUnique(out_texture_paths, out_texture_path.C_Str());
    }
    if (aiGetMaterialTexture(&ai_material, aiTextureType_NORMALS, 0, &out_texture_path, &out_texture_mapping, &out_uv_index, &out_blend,
                             &out_texture_op, out_texture_mode.data(), &out_texture_flags) == AI_SUCCESS)
    {
        out_description.normal_texture = AddUnique(out_texture_paths, out_texture_path.C_Str());
    }
    // In case that there is no normal map, try to read the height map that can be later converted into a normal map.
    if (out_description.normal_texture == k_invalid_image_id)
    {
        if (aiGetMaterialTexture(&ai_material, aiTextureType_HEIGHT, 0, &out_texture_path, &out_texture_mapping, &out_uv_index, &out_blend,
                                 &out_texture_op, out_texture_mode.data(), &out_texture_flags) == AI_SUCCESS)
        {
            out_description.normal_texture = AddUnique(out_texture_paths, out_texture_path.C_Str());
        }
    }
    if (aiGetMaterialTexture(&ai_material, aiTextureType_OPACITY, 0, &out_texture_path, &out_texture_mapping, &out_uv_index, &out_blend,
                             &out_texture_op, out_texture_mode.data(), &out_texture_flags) == AI_SUCCESS)
    {
        // Opacity info will later be stored in the alpha channel of the albedo map.
        out_description.opacity_texture = AddUnique(out_opacity_maps, out_texture_path.C_Str());
        out_description.alpha_test = 0.5f;
    }

    // Material heuristics, modify material parameters based on the texture name so that it looks better.
    aiString ai_material_name;
    Opal::StringUtf8 material_name;
    if (aiGetMaterialString(&ai_material, AI_MATKEY_NAME, &ai_material_name) == AI_SUCCESS)
    {
        material_name = reinterpret_cast<const c8*>(ai_material_name.C_Str());
    }
    if ((Opal::Find(material_name, OPAL_UTF8("Glass")) != Opal::StringUtf8::k_npos) ||
        (Opal::Find(material_name, OPAL_UTF8("Vespa_Headlight")) != Opal::StringUtf8::k_npos))
    {
        out_description.alpha_test = 0.75f;
        out_description.transparency_factor = 0.1f;
        out_description.flags |= MaterialFlags::Transparent;
    }
    else if (Opal::Find(material_name, OPAL_UTF8("Bottle")) != Opal::StringUtf8::k_npos)
    {
        out_description.alpha_test = 0.54f;
        out_description.transparency_factor = 0.4f;
        out_description.flags |= MaterialFlags::Transparent;
    }
    else if (Opal::Find(material_name, OPAL_UTF8("Metal")) != Opal::StringUtf8::k_npos)
    {
        out_description.metallic_factor = 1.0f;
        out_description.roughness = Vector4f(0.1f, 0.1f, 0.0f, 0.0f);
    }

    RNDR_LOG_DEBUG("Texture paths: %d", out_texture_paths.GetSize());
    for (const Opal::StringUtf8& texture_path : out_texture_paths)
    {
        RNDR_LOG_DEBUG("\t%s", texture_path.GetDataAs<c>());
    }

    RNDR_LOG_DEBUG("Opacity maps: %d", out_opacity_maps.GetSize());
    for (const Opal::StringUtf8& out_opacity_map : out_opacity_maps)
    {
        RNDR_LOG_DEBUG("\t%s", out_opacity_map.GetDataAs<c>());
    }

    return true;
}

bool AssimpHelpers::ReadSceneDescription(SceneDescription& out_scene_description, const aiScene& ai_scene)
{
    Traverse(out_scene_description, &ai_scene, ai_scene.mRootNode, Scene::k_invalid_node_id, 0);

    for (u32 i = 0; i < ai_scene.mNumMaterials; ++i)
    {
        aiMaterial* ai_material = ai_scene.mMaterials[i];
        const Opal::StringUtf8 material_name = reinterpret_cast<const c8*>(ai_material->GetName().C_Str());
        out_scene_description.material_names.PushBack(material_name);
    }

    return true;
}
Rndr::ErrorCode AssimpHelpers::ReadAnimationDataFromAssimp(SkeletalMeshData& out_skeletal_mesh, const Opal::StringUtf8& mesh_file_path)
{
    constexpr u32 k_ai_process_flags = aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                                       aiProcess_LimitBoneWeights | aiProcess_SplitLargeMeshes | aiProcess_ImproveCacheLocality |
                                       aiProcess_RemoveRedundantMaterials | aiProcess_FindDegenerates | aiProcess_FindInvalidData |
                                       aiProcess_GenUVCoords;

    const aiScene* ai_scene = aiImportFile(mesh_file_path.GetDataAs<c>(), k_ai_process_flags);
    if (ai_scene == nullptr || !ai_scene->HasMeshes())
    {
        RNDR_LOG_ERROR("Failed to load mesh from file with error: %s", aiGetErrorString());
        return Rndr::ErrorCode::InvalidArgument;
    }

    if (!ai_scene->HasMeshes())
    {
        RNDR_LOG_ERROR("No meshes in the assimp scene!");
        return Rndr::ErrorCode::InvalidArgument;
    }

    struct Node
    {
        aiNode* ai_node;
        i32 depth = 0;
    };
    std::stack<Node> nodes;
    nodes.push({ai_scene->mRootNode});
    while (!nodes.empty())
    {
        Node node = nodes.top();
        nodes.pop();
        const Opal::StringUtf8 prefix(node.depth, '\t');
        RNDR_LOG_INFO("%sNode: %s", prefix.GetDataAs<c>(), node.ai_node->mName.C_Str());
        for (u32 i = 0; i < node.ai_node->mNumChildren; ++i)
        {
            nodes.push({node.ai_node->mChildren[i], node.depth + 1});
        }
    }

    constexpr u32 k_max_bone_influence_count = 4;

    u32 vertex_size = sizeof(Rndr::Point3f);
    vertex_size += sizeof(Rndr::Normal3f);
    vertex_size += sizeof(Rndr::Point2f);
    vertex_size += sizeof(i32) * k_max_bone_influence_count;
    vertex_size += sizeof(float) * k_max_bone_influence_count;

    u32 vertex_offset = 0;
    u32 index_offset = 0;
    for (u32 mesh_index = 0; mesh_index < ai_scene->mNumMeshes; ++mesh_index)
    {
        const aiMesh* const ai_mesh = ai_scene->mMeshes[mesh_index];

        struct VertexByBoneInfluence
        {
            Opal::StackArray<i32, k_max_bone_influence_count> bone_ids;
            Opal::StackArray<f32, k_max_bone_influence_count> bone_weights;
        };
        VertexByBoneInfluence default_vertex_by_bone_influence;
        default_vertex_by_bone_influence.bone_ids.fill(-1);
        default_vertex_by_bone_influence.bone_weights.fill(0.0f);
        Opal::Array<VertexByBoneInfluence> vertex_by_bone_influences(ai_mesh->mNumVertices, default_vertex_by_bone_influence);

        for (i32 bone_id = 0; bone_id < static_cast<i32>(ai_mesh->mNumBones); ++bone_id)
        {
            const aiBone& ai_bone = *ai_mesh->mBones[bone_id];
            for (i32 weight_index = 0; weight_index < static_cast<i32>(ai_bone.mNumWeights); ++weight_index)
            {
                const aiVertexWeight& ai_weight = ai_bone.mWeights[weight_index];
                VertexByBoneInfluence& vertex_by_bone_influence = vertex_by_bone_influences[ai_weight.mVertexId];
                for (i32 i = 0; i < k_max_bone_influence_count; ++i)
                {
                    if (vertex_by_bone_influence.bone_ids[i] == -1)
                    {
                        vertex_by_bone_influence.bone_ids[i] = bone_id;
                        vertex_by_bone_influence.bone_weights[i] = ai_weight.mWeight;
                        break;
                    }
                }
            }
        }

        for (u32 i = 0; i < ai_mesh->mNumVertices; ++i)
        {
            Rndr::Point3f position(ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z);
            out_skeletal_mesh.vertex_buffer_data.Insert(out_skeletal_mesh.vertex_buffer_data.ConstEnd(),
                                                        reinterpret_cast<u8*>(position.data),
                                                        reinterpret_cast<u8*>(position.data) + sizeof(position));

            RNDR_ASSERT(ai_mesh->HasNormals());
            Rndr::Normal3f normal(ai_mesh->mNormals[i].x, ai_mesh->mNormals[i].y, ai_mesh->mNormals[i].z);
            out_skeletal_mesh.vertex_buffer_data.Insert(out_skeletal_mesh.vertex_buffer_data.ConstEnd(),
                                                        reinterpret_cast<uint8_t*>(normal.data),
                                                        reinterpret_cast<uint8_t*>(normal.data) + sizeof(normal));

            const aiVector3D ai_uv = ai_mesh->HasTextureCoords(0) ? ai_mesh->mTextureCoords[0][i] : aiVector3D();
            Rndr::Point2f uv(ai_uv.x, ai_uv.y);
            out_skeletal_mesh.vertex_buffer_data.Insert(out_skeletal_mesh.vertex_buffer_data.ConstEnd(),
                                                        reinterpret_cast<uint8_t*>(uv.data),
                                                        reinterpret_cast<uint8_t*>(uv.data) + sizeof(uv));

            VertexByBoneInfluence& vertex_by_bone_influence = vertex_by_bone_influences[i];
            out_skeletal_mesh.vertex_buffer_data.Insert(
                out_skeletal_mesh.vertex_buffer_data.ConstEnd(), reinterpret_cast<uint8_t*>(&vertex_by_bone_influence),
                reinterpret_cast<uint8_t*>(&vertex_by_bone_influence) + sizeof(vertex_by_bone_influences));
        }

        Opal::Array<Opal::Array<u32>> lods(MeshDescription::k_max_lods);
        for (u32 i = 0; i < ai_mesh->mNumFaces; ++i)
        {
            const aiFace& face = ai_mesh->mFaces[i];
            if (face.mNumIndices != 3)
            {
                continue;
            }
            for (u32 j = 0; j < face.mNumIndices; ++j)
            {
                lods[0].PushBack(face.mIndices[j]);
            }
        }

        out_skeletal_mesh.index_buffer_data.Insert(out_skeletal_mesh.index_buffer_data.ConstEnd(),
                                                   reinterpret_cast<uint8_t*>(lods[0].GetData()),
                                                   reinterpret_cast<uint8_t*>(lods[0].GetData()) + lods[0].GetSize() * sizeof(u32));

        // TODO: Generate LODs

        SkeletalMeshDescription mesh_desc;
        mesh_desc.vertex_count = ai_mesh->mNumVertices;
        mesh_desc.vertex_offset = vertex_offset;
        mesh_desc.vertex_size = vertex_size;
        mesh_desc.index_offset = index_offset;
        mesh_desc.lod_count = 1;
        mesh_desc.lod_offsets[0] = 0;
        mesh_desc.lod_offsets[1] = static_cast<u32>(lods[0].GetSize());
        mesh_desc.mesh_size = ai_mesh->mNumVertices * vertex_size + static_cast<u32>(lods[0].GetSize()) * sizeof(u32);

        // TODO: Add material info

        out_skeletal_mesh.meshes.PushBack(mesh_desc);

        vertex_offset += ai_mesh->mNumVertices;
        index_offset += static_cast<u32>(lods[0].GetSize());
    }

    return Rndr::ErrorCode::Success;
}

namespace
{

Opal::StringUtf8 NumberToStr(i32 number)
{
    return reinterpret_cast<const c8*>(std::to_string(number).c_str());
}

void Traverse(SceneDescription& out_scene, const aiScene* ai_scene, const aiNode* ai_node, Scene::NodeId parent, int32_t level)
{
    const Scene::NodeId new_node_id = Scene::AddNode(out_scene, parent, level);

    Opal::StringUtf8 node_name = reinterpret_cast<const c8*>(ai_node->mName.C_Str());
    if (node_name.IsEmpty())
    {
        node_name = OPAL_UTF8("Node_") + NumberToStr(new_node_id);
    }
    Scene::SetNodeName(out_scene, new_node_id, node_name);

    for (u32 i = 0; i < ai_node->mNumMeshes; ++i)
    {
        const Scene::NodeId new_sub_node_id = Scene::AddNode(out_scene, new_node_id, level + 1);
        Scene::SetNodeName(out_scene, new_sub_node_id, node_name + OPAL_UTF8("_Mesh_") + NumberToStr(i));
        const u32 mesh_id = ai_node->mMeshes[i];
        Scene::SetNodeMeshId(out_scene, new_sub_node_id, mesh_id);
        Scene::SetNodeMaterialId(out_scene, new_sub_node_id, ai_scene->mMeshes[mesh_id]->mMaterialIndex);

        out_scene.local_transforms[new_sub_node_id] = Rndr::Matrix4x4f(1.0f);
        out_scene.world_transforms[new_sub_node_id] = Rndr::Matrix4x4f(1.0f);
    }

    out_scene.local_transforms[new_node_id] = AssimpHelpers::Convert(ai_node->mTransformation);
    out_scene.world_transforms[new_node_id] = Rndr::Matrix4x4f(1.0f);

    for (u32 i = 0; i < ai_node->mNumChildren; ++i)
    {
        Traverse(out_scene, ai_scene, ai_node->mChildren[i], new_node_id, level + 1);
    }
}
}  // namespace
