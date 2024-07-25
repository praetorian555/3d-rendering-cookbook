#include "rndr/utility/assimp-helpers.h"

#include <assimp/cimport.h>
#include <assimp/material.h>
#include <assimp/pbrmaterial.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "rndr/utility/material.h"
#include "rndr/utility/scene.h"

namespace
{
void Traverse(Rndr::SceneDescription& out_scene, const aiScene* ai_scene, const aiNode* ai_node, Rndr::Scene::NodeId parent, int32_t level);
}

Rndr::Matrix4x4f Rndr::AssimpHelpers::Convert(const aiMatrix4x4& ai_matrix)
{
    return {ai_matrix.a1, ai_matrix.a2, ai_matrix.a3, ai_matrix.a4, ai_matrix.b1, ai_matrix.b2, ai_matrix.b3, ai_matrix.b4,
            ai_matrix.c1, ai_matrix.c2, ai_matrix.c3, ai_matrix.c4, ai_matrix.d1, ai_matrix.d2, ai_matrix.d3, ai_matrix.d4};
}

bool Rndr::AssimpHelpers::ReadMeshData(MeshData& out_mesh_data, const aiScene& ai_scene, MeshAttributesToLoad attributes_to_load)
{
    if (!ai_scene.HasMeshes())
    {
        RNDR_LOG_ERROR("No meshes in the assimp scene!");
        return false;
    }

    const bool should_load_normals = static_cast<bool>(attributes_to_load & MeshAttributesToLoad::LoadNormals);
    const bool should_load_uvs = static_cast<bool>(attributes_to_load & MeshAttributesToLoad::LoadUvs);
    uint32_t vertex_size = sizeof(Rndr::Point3f);
    if (should_load_normals)
    {
        vertex_size += sizeof(Rndr::Normal3f);
    }
    if (should_load_uvs)
    {
        vertex_size += sizeof(Rndr::Point2f);
    }

    uint32_t vertex_offset = 0;
    uint32_t index_offset = 0;
    for (uint32_t mesh_index = 0; mesh_index < ai_scene.mNumMeshes; ++mesh_index)
    {
        const aiMesh* const ai_mesh = ai_scene.mMeshes[mesh_index];

        for (uint32_t i = 0; i < ai_mesh->mNumVertices; ++i)
        {
            Rndr::Point3f position(ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z);
            out_mesh_data.vertex_buffer_data.insert(out_mesh_data.vertex_buffer_data.end(), reinterpret_cast<uint8_t*>(position.data),
                                                    reinterpret_cast<uint8_t*>(position.data) + sizeof(position));

            if (should_load_normals)
            {
                RNDR_ASSERT(ai_mesh->HasNormals());
                Rndr::Normal3f normal(ai_mesh->mNormals[i].x, ai_mesh->mNormals[i].y, ai_mesh->mNormals[i].z);
                out_mesh_data.vertex_buffer_data.insert(out_mesh_data.vertex_buffer_data.end(), reinterpret_cast<uint8_t*>(normal.data),
                                                        reinterpret_cast<uint8_t*>(normal.data) + sizeof(normal));
            }
            if (should_load_uvs)
            {
                aiVector3D ai_uv = ai_mesh->HasTextureCoords(0) ? ai_mesh->mTextureCoords[0][i] : aiVector3D();
                Rndr::Point2f uv(ai_uv.x, ai_uv.y);
                out_mesh_data.vertex_buffer_data.insert(out_mesh_data.vertex_buffer_data.end(), reinterpret_cast<uint8_t*>(uv.data),
                                                        reinterpret_cast<uint8_t*>(uv.data) + sizeof(uv));
            }
        }

        Rndr::Array<Rndr::Array<uint32_t>> lods(MeshDescription::k_max_lods);
        for (uint32_t i = 0; i < ai_mesh->mNumFaces; ++i)
        {
            const aiFace& face = ai_mesh->mFaces[i];
            if (face.mNumIndices != 3)
            {
                continue;
            }
            for (uint32_t j = 0; j < face.mNumIndices; ++j)
            {
                lods[0].emplace_back(face.mIndices[j]);
            }
        }

        out_mesh_data.index_buffer_data.insert(out_mesh_data.index_buffer_data.end(), reinterpret_cast<uint8_t*>(lods[0].data()),
                                               reinterpret_cast<uint8_t*>(lods[0].data()) + lods[0].size() * sizeof(uint32_t));

        // TODO: Generate LODs

        MeshDescription mesh_desc;
        mesh_desc.vertex_count = ai_mesh->mNumVertices;
        mesh_desc.vertex_offset = vertex_offset;
        mesh_desc.vertex_size = vertex_size;
        mesh_desc.index_offset = index_offset;
        mesh_desc.lod_count = 1;
        mesh_desc.lod_offsets[0] = 0;
        mesh_desc.lod_offsets[1] = static_cast<uint32_t>(lods[0].size());
        mesh_desc.mesh_size = ai_mesh->mNumVertices * vertex_size + static_cast<uint32_t>(lods[0].size()) * sizeof(uint32_t);

        // TODO: Add material info

        out_mesh_data.meshes.emplace_back(mesh_desc);

        vertex_offset += ai_mesh->mNumVertices;
        index_offset += static_cast<uint32_t>(lods[0].size());
    }

    Mesh::UpdateBoundingBoxes(out_mesh_data);

    return true;
}

bool Rndr::AssimpHelpers::ReadMeshData(Rndr::MeshData& out_mesh_data, const Rndr::String& mesh_file_path,
                                       Rndr::MeshAttributesToLoad attributes_to_load)
{
    constexpr uint32_t k_ai_process_flags = aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                                            aiProcess_LimitBoneWeights | aiProcess_SplitLargeMeshes | aiProcess_ImproveCacheLocality |
                                            aiProcess_RemoveRedundantMaterials | aiProcess_FindDegenerates | aiProcess_FindInvalidData |
                                            aiProcess_GenUVCoords;

    const aiScene* scene = aiImportFile(mesh_file_path.c_str(), k_ai_process_flags);
    if (scene == nullptr || !scene->HasMeshes())
    {
        RNDR_LOG_ERROR("Failed to load mesh from file with error: %s", aiGetErrorString());
        return false;
    }

    if (!ReadMeshData(out_mesh_data, *scene, attributes_to_load))
    {
        RNDR_LOG_ERROR("Failed to load mesh data from file: %s", mesh_file_path.c_str());
        return false;
    }

    aiReleaseImport(scene);

    return true;
}

namespace
{
Rndr::ImageId AddUnique(Rndr::Array<Rndr::String>& files, const char* path)
{
    for (uint32_t i = 0; i < files.size(); ++i)
    {
        if (files[i] == path)
        {
            return i;
        }
    }
    files.emplace_back(path);
    return files.size() - 1;
}
}  // namespace

bool Rndr::AssimpHelpers::ReadMaterialDescription(MaterialDescription& out_description, Rndr::Array<Rndr::String>& out_texture_paths,
                                                  Rndr::Array<Rndr::String>& out_opacity_maps, const aiMaterial& ai_material)
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
    StackArray<aiTextureMapMode, 2> out_texture_mode = {aiTextureMapMode_Wrap, aiTextureMapMode_Wrap};
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
        const String albedo_map_path = out_texture_path.C_Str();
        if (albedo_map_path.find("grey_30") != String::npos)
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
    String material_name;
    if (aiGetMaterialString(&ai_material, AI_MATKEY_NAME, &ai_material_name) == AI_SUCCESS)
    {
        material_name = ai_material_name.C_Str();
    }
    if ((material_name.find("Glass") != String::npos) || (material_name.find("Vespa_Headlight") != String::npos))
    {
        out_description.alpha_test = 0.75f;
        out_description.transparency_factor = 0.1f;
        out_description.flags |= MaterialFlags::Transparent;
    }
    else if (material_name.find("Bottle") != String::npos)
    {
        out_description.alpha_test = 0.54f;
        out_description.transparency_factor = 0.4f;
        out_description.flags |= MaterialFlags::Transparent;
    }
    else if (material_name.find("Metal") != String::npos)
    {
        out_description.metallic_factor = 1.0f;
        out_description.roughness = Vector4f(0.1f, 0.1f, 0.0f, 0.0f);
    }

    RNDR_LOG_DEBUG("Texture paths: %d", out_texture_paths.size());
    for (const String& texture_path : out_texture_paths)
    {
        RNDR_LOG_DEBUG("\t%s", texture_path.c_str());
    }

    RNDR_LOG_DEBUG("Opacity maps: %d", out_opacity_maps.size());
    for (const String& out_opacity_map : out_opacity_maps)
    {
        RNDR_LOG_DEBUG("\t%s", out_opacity_map.c_str());
    }

    return true;
}

bool Rndr::AssimpHelpers::ReadSceneDescription(Rndr::SceneDescription& out_scene_description, const aiScene& ai_scene)
{
    Traverse(out_scene_description, &ai_scene, ai_scene.mRootNode, Rndr::Scene::k_invalid_node_id, 0);

    for (uint32_t i = 0; i < ai_scene.mNumMaterials; ++i)
    {
        aiMaterial* ai_material = ai_scene.mMaterials[i];
        out_scene_description.material_names.emplace_back(ai_material->GetName().C_Str());
    }

    return true;
}

namespace
{
void Traverse(Rndr::SceneDescription& out_scene, const aiScene* ai_scene, const aiNode* ai_node, Rndr::Scene::NodeId parent, int32_t level)
{
    const Rndr::Scene::NodeId new_node_id = Rndr::Scene::AddNode(out_scene, parent, level);

    Rndr::String node_name = ai_node->mName.C_Str();
    if (node_name.empty())
    {
        node_name = "Node_" + std::to_string(new_node_id);
    }
    Rndr::Scene::SetNodeName(out_scene, new_node_id, node_name);

    for (uint32_t i = 0; i < ai_node->mNumMeshes; ++i)
    {
        const Rndr::Scene::NodeId new_sub_node_id = Rndr::Scene::AddNode(out_scene, new_node_id, level + 1);
        Rndr::Scene::SetNodeName(out_scene, new_sub_node_id, node_name + "_Mesh_" + std::to_string(i));
        const uint32_t mesh_id = ai_node->mMeshes[i];
        Rndr::Scene::SetNodeMeshId(out_scene, new_sub_node_id, mesh_id);
        Rndr::Scene::SetNodeMaterialId(out_scene, new_sub_node_id, ai_scene->mMeshes[mesh_id]->mMaterialIndex);

        out_scene.local_transforms[new_sub_node_id] = Rndr::Matrix4x4f(1.0f);
        out_scene.world_transforms[new_sub_node_id] = Rndr::Matrix4x4f(1.0f);
    }

    out_scene.local_transforms[new_node_id] = Rndr::AssimpHelpers::Convert(ai_node->mTransformation);
    out_scene.world_transforms[new_node_id] = Rndr::Matrix4x4f(1.0f);

    for (uint32_t i = 0; i < ai_node->mNumChildren; ++i)
    {
        Traverse(out_scene, ai_scene, ai_node->mChildren[i], new_node_id, level + 1);
    }
}
} // namespace
