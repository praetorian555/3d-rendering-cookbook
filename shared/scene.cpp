#include "scene.h"

#include <stack>

#include "rndr/file.h"

namespace
{
bool WriteMap(Rndr::FileHandler& file, const Opal::HashMap<Scene::NodeId, uint32_t>& map)
{
    Opal::Array<uint32_t> flattened_map;
    flattened_map.Reserve(map.size() * 2);

    for (const auto& pair : map)
    {
        flattened_map.PushBack(pair.first);
        flattened_map.PushBack(pair.second);
    }

    const size_t flattened_map_size = flattened_map.GetSize();
    file.Write(&flattened_map_size, sizeof(flattened_map_size), 1);
    if (flattened_map_size == 0)
    {
        return true;
    }

    file.Write(flattened_map.GetData(), sizeof(flattened_map[0]), flattened_map.GetSize());
    return true;
}

bool ReadMap(Rndr::FileHandler& file, Opal::HashMap<Scene::NodeId, uint32_t>& map)
{
    size_t flattened_map_size = 0;
    file.Read(&flattened_map_size, sizeof(flattened_map_size), 1);
    if (flattened_map_size == 0)
    {
        return true;
    }

    Opal::Array<uint32_t> flattened_map(flattened_map_size);
    file.Read(flattened_map.GetData(), sizeof(uint32_t), flattened_map.GetSize());
    for (uint32_t i = 0; i < flattened_map_size; i += 2)
    {
        map[flattened_map[i]] = flattened_map[i + 1];
    }
    return true;
}

bool WriteStringList(Rndr::FileHandler& file, const Opal::Array<Opal::StringUtf8>& strings)
{
    const size_t string_count = strings.GetSize();
    file.Write(&string_count, sizeof(string_count), 1);
    for (const auto& string : strings)
    {
        const size_t string_length = string.GetSize();
        file.Write(&string_length, sizeof(string_length), 1);
        file.Write(string.GetData(), string_length + 1, 1);
    }
    return true;
}

bool ReadStringList(Rndr::FileHandler& file, Opal::Array<Opal::StringUtf8>& strings)
{
    size_t string_count = 0;
    file.Read(&string_count, sizeof(string_count), 1);
    strings.Resize(string_count);
    for (auto& string : strings)
    {
        size_t string_length = 0;
        file.Read(&string_length, sizeof(string_length), 1);
        Opal::Array<c8> in_bytes(string_length + 1);
        file.Read(in_bytes.GetData(), string_length + 1, 1);
        string = Opal::StringUtf8(in_bytes.GetData(), in_bytes.GetSize());
    }
    return true;
}

};  // namespace

bool Scene::ReadSceneDescription(SceneDescription& out_scene_description, const Opal::StringUtf8& scene_file)
{
    const char* scene_file_raw = reinterpret_cast<const char*>(scene_file.GetData());
    Rndr::FileHandler file(scene_file_raw, "rb");
    if (!file.IsValid())
    {
        return false;
    }

    size_t node_count = 0;
    file.Read(&node_count, sizeof(node_count), 1);

    if (node_count != 0)
    {
        out_scene_description.local_transforms.Resize(node_count);
        out_scene_description.world_transforms.Resize(node_count);
        out_scene_description.hierarchy.Resize(node_count);
        file.Read(out_scene_description.local_transforms.GetData(), sizeof(out_scene_description.local_transforms[0]), node_count);
        file.Read(out_scene_description.world_transforms.GetData(), sizeof(out_scene_description.world_transforms[0]), node_count);
        file.Read(out_scene_description.hierarchy.GetData(), sizeof(out_scene_description.hierarchy[0]), node_count);
    }

    ReadMap(file, out_scene_description.node_id_to_mesh_id);
    ReadMap(file, out_scene_description.node_id_to_material_id);

    if (!file.IsEOF())
    {
        ReadMap(file, out_scene_description.node_id_to_name);
        ReadStringList(file, out_scene_description.node_names);
        ReadStringList(file, out_scene_description.material_names);
    }

    return true;
}

bool Scene::WriteSceneDescription(const SceneDescription& scene_description, const Opal::StringUtf8& scene_file)
{
    const char* scene_file_raw = reinterpret_cast<const char*>(scene_file.GetData());
    Rndr::FileHandler file(scene_file_raw, "wb");
    if (!file.IsValid())
    {
        return false;
    }

    const size_t node_count = scene_description.hierarchy.GetSize();
    file.Write(&node_count, sizeof(node_count), 1);

    if (node_count != 0)
    {
        file.Write(scene_description.local_transforms.GetData(), sizeof(scene_description.local_transforms[0]), node_count);
        file.Write(scene_description.world_transforms.GetData(), sizeof(scene_description.world_transforms[0]), node_count);
        file.Write(scene_description.hierarchy.GetData(), sizeof(scene_description.hierarchy[0]), node_count);
    }

    WriteMap(file, scene_description.node_id_to_mesh_id);
    WriteMap(file, scene_description.node_id_to_material_id);

    if (!scene_description.node_id_to_name.empty() && !scene_description.node_names.IsEmpty())
    {
        WriteMap(file, scene_description.node_id_to_name);
        WriteStringList(file, scene_description.node_names);
        WriteStringList(file, scene_description.material_names);
    }

    return true;
}

bool Scene::ReadScene(SceneDrawData& out_scene, const Opal::StringUtf8& scene_file, const Opal::StringUtf8& mesh_file,
                            const Opal::StringUtf8& material_file, const Rndr::GraphicsContext& graphics_context)
{
    if (!ReadSceneDescription(out_scene.scene_description, scene_file))
    {
        return false;
    }

    if (!Mesh::ReadData(out_scene.mesh_data, mesh_file))
    {
        return false;
    }

    if (!Material::ReadDataLoadTextures(out_scene.materials, out_scene.textures, material_file, graphics_context))
    {
        return false;
    }

    for (const auto& node : out_scene.scene_description.node_id_to_mesh_id)
    {
        const Scene::NodeId node_id = node.first;
        const uint32_t mesh_id = node.second;
        const auto material_iter = out_scene.scene_description.node_id_to_material_id.find(node_id);
        if (material_iter == out_scene.scene_description.node_id_to_material_id.end())
        {
            continue;
        }
        const uint32_t material_id = material_iter->second;
        out_scene.shapes.PushBack({.mesh_index = mesh_id,
                                   .material_index = material_id,
                                   .lod = 0,
                                   .vertex_buffer_offset = out_scene.mesh_data.meshes[mesh_id].vertex_offset,
                                   .index_buffer_offset = out_scene.mesh_data.meshes[mesh_id].index_offset,
                                   .transform_index = node_id});
    }

    // Mark root as changed so that whole hierarchy is recalculated
    Scene::MarkAsChanged(out_scene.scene_description, 0);
    Scene::RecalculateWorldTransforms(out_scene.scene_description);

    return true;
}

Scene::NodeId Scene::AddNode(SceneDescription& scene, int32_t parent, int32_t level)
{
    const NodeId node_id = static_cast<NodeId>(scene.hierarchy.GetSize());
    scene.local_transforms.PushBack(Matrix4x4f(1.0f));
    scene.world_transforms.PushBack(Matrix4x4f(1.0f));
    scene.hierarchy.PushBack(HierarchyNode{.parent = parent, .last_sibling = -1, .level = level});

    if (parent > -1)
    {
        const NodeId parent_first_child = scene.hierarchy[parent].first_child;
        if (parent_first_child == k_invalid_node_id)
        {
            scene.hierarchy[parent].first_child = node_id;
            scene.hierarchy[node_id].last_sibling = node_id;
        }
        else
        {
            NodeId last_sibling = scene.hierarchy[parent_first_child].last_sibling;
            if (last_sibling == k_invalid_node_id)
            {
                for (last_sibling = parent_first_child; scene.hierarchy[last_sibling].next_sibling != k_invalid_node_id;
                     last_sibling = scene.hierarchy[last_sibling].next_sibling)
                {
                }
            }
            scene.hierarchy[last_sibling].next_sibling = node_id;
            scene.hierarchy[parent_first_child].last_sibling = node_id;
        }
    }

    scene.hierarchy[node_id].level = level;

    return node_id;
}

void Scene::SetNodeName(SceneDescription& scene, Scene::NodeId node, const Opal::StringUtf8& name)
{
    RNDR_ASSERT(IsValidNodeId(scene, node));
    scene.node_id_to_name[node] = static_cast<uint32_t>(scene.node_names.GetSize());
    scene.node_names.PushBack(name);
}

bool Scene::IsValidNodeId(const SceneDescription& scene, Scene::NodeId node)
{
    return node < scene.hierarchy.GetSize();
}

void Scene::SetNodeMeshId(SceneDescription& scene, Scene::NodeId node, uint32_t mesh_id)
{
    RNDR_ASSERT(IsValidNodeId(scene, node));
    scene.node_id_to_mesh_id[node] = mesh_id;
}

void Scene::SetNodeMaterialId(SceneDescription& scene, Scene::NodeId node, uint32_t material_id)
{
    RNDR_ASSERT(IsValidNodeId(scene, node));
    scene.node_id_to_material_id[node] = material_id;
}

void Scene::MarkAsChanged(SceneDescription& scene, Scene::NodeId node)
{
    std::stack<Scene::NodeId> stack;
    stack.push(node);

    while (!stack.empty())
    {
        const NodeId node_to_mark = stack.top();
        stack.pop();
        RNDR_ASSERT(IsValidNodeId(scene, node_to_mark));

        const int32_t level = scene.hierarchy[node_to_mark].level;
        scene.dirty_nodes[level].PushBack(node_to_mark);

        for (NodeId child = scene.hierarchy[node_to_mark].first_child; child != k_invalid_node_id;
             child = scene.hierarchy[child].next_sibling)
        {
            stack.push(child);
        }
    }
}

void Scene::RecalculateWorldTransforms(SceneDescription& scene)
{
    // Process root level first
    if (!scene.dirty_nodes[0].IsEmpty())
    {
        const NodeId root_node = scene.dirty_nodes[0].Back().GetValue();
        scene.world_transforms[root_node] = scene.local_transforms[root_node];
        scene.dirty_nodes[0].Clear();
    }

    for (int i = 1; i < k_max_node_level && !scene.dirty_nodes[i].IsEmpty(); ++i)
    {
        for (const NodeId node : scene.dirty_nodes[i])
        {
            const NodeId parent = scene.hierarchy[node].parent;
            scene.world_transforms[node] = scene.world_transforms[parent] * scene.local_transforms[node];
        }
        scene.dirty_nodes[i].Clear();
    }
}
