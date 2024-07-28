#include <filesystem>

#include <imgui.h>

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <gli/gli.hpp>
#include <gli/load_ktx.hpp>
#include <gli/texture2d.hpp>

#include "opal/container/scope-ptr.h"
#include "opal/container/string.h"
#include "opal/paths.h"

#include "rndr/core/file.h"
#include "rndr/core/render-api.h"
#include "rndr/core/renderer-base.h"
#include "rndr/core/window.h"

#include "assimp-helpers.h"
#include "cube-map.h"
#include "imgui-wrapper.h"
#include "mesh.h"
#include "scene.h"

class UIRenderer : public Rndr::RendererBase
{
public:
    UIRenderer(const Opal::StringUtf8& name, Rndr::Window& window, const Rndr::RendererBaseDesc& desc);
    ~UIRenderer();

    bool Render() override;

private:
    void RenderMeshConverterTool();
    void RenderComputeBrdfLutTool();
    void RenderComputeEnvironmentMapTool();

    void ProcessScene(const Opal::StringUtf8& in_mesh_path, const Opal::StringUtf8& out_scene_path, const Opal::StringUtf8& out_mesh_path,
                      const Opal::StringUtf8& out_material_path, MeshAttributesToLoad attributes_to_load, Opal::StringUtf8& out_status);
    void ComputeBrdfLut(const Opal::StringUtf8& output_path, Opal::StringUtf8& status);
    void ComputeEnvironmentMap(const Opal::StringUtf8& input_path, const Opal::StringUtf8& output_path, Opal::StringUtf8& status);

    Rndr::Buffer m_brdf_lut_buffer;
    Rndr::Shader m_brdf_lut_shader;
    Rndr::Pipeline m_brdf_lut_pipeline;
    int32_t m_brdf_lut_width = 256;
    int32_t m_brdf_lut_height = 256;
};

void Run();
Opal::StringUtf8 OpenFileDialog();
Opal::StringUtf8 OpenFolderDialog();

int main()
{
    Rndr::Init();
    Run();
    Rndr::Destroy();
    return 0;
}

void Run()
{
    Rndr::WindowDesc window_desc;
    window_desc.name = "Converters";
    window_desc.width = 1280;
    window_desc.height = 720;

    Rndr::Window window(window_desc);
    Rndr::GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});
    Rndr::SwapChain swap_chain(graphics_context, {});

    Rndr::RendererBaseDesc renderer_desc;
    renderer_desc.graphics_context = graphics_context;
    renderer_desc.swap_chain = swap_chain;

    Rndr::RendererManager renderer_manager;
    const Opal::ScopePtr<Rndr::RendererBase> clear_renderer =
        Opal::MakeDefaultScoped<Rndr::ClearRenderer>(u8"Clear", renderer_desc, Rndr::Vector4f(0.0f, 0.0f, 0.0f, 1.0f));
    const Opal::ScopePtr<Rndr::RendererBase> ui_renderer = Opal::MakeDefaultScoped<UIRenderer>(u8"UI", window, renderer_desc);
    const Opal::ScopePtr<Rndr::RendererBase> present_renderer = Opal::MakeDefaultScoped<Rndr::PresentRenderer>(u8"Present", renderer_desc);
    renderer_manager.AddRenderer(clear_renderer.get());
    renderer_manager.AddRenderer(ui_renderer.get());
    renderer_manager.AddRenderer(present_renderer.get());

    while (!window.IsClosed())
    {
        window.ProcessEvents();
        renderer_manager.Render();
    }
}

UIRenderer::UIRenderer(const Opal::StringUtf8& name, Rndr::Window& window, const Rndr::RendererBaseDesc& desc) : RendererBase(name, desc)
{
    ImGuiWrapper::Init(window, *desc.graphics_context, {.display_demo_window = false});

    const uint32_t buffer_size = m_brdf_lut_width * m_brdf_lut_height * sizeof(float) * 2;
    m_brdf_lut_buffer =
        Rndr::Buffer(desc.graphics_context,
                     Rndr::BufferDesc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = buffer_size});
    RNDR_ASSERT(m_brdf_lut_buffer.IsValid());
    const Opal::StringUtf8 shaders_path = Opal::Paths::Combine(nullptr, OPAL_UTF8(ASSETS_ROOT), OPAL_UTF8("shaders")).GetValue();
    const Opal::StringUtf8 shader_source = Rndr::File::ReadShader(shaders_path, u8"compute-brdf.glsl");
    RNDR_ASSERT(!shader_source.IsEmpty());
    m_brdf_lut_shader = Rndr::Shader(desc.graphics_context, Rndr::ShaderDesc{.type = Rndr::ShaderType::Compute, .source = shader_source});
    RNDR_ASSERT(m_brdf_lut_shader.IsValid());
    m_brdf_lut_pipeline = Rndr::Pipeline(desc.graphics_context, Rndr::PipelineDesc{.compute_shader = &m_brdf_lut_shader});
    RNDR_ASSERT(m_brdf_lut_pipeline.IsValid());
}

UIRenderer::~UIRenderer()
{
    ImGuiWrapper::Destroy();
}

bool UIRenderer::Render()
{
    ImGuiWrapper::StartFrame();
    RenderMeshConverterTool();
    RenderComputeBrdfLutTool();
    RenderComputeEnvironmentMapTool();
    ImGuiWrapper::EndFrame();
    return true;
}

#define GLTF_SAMPLE_ASSETS OPAL_UTF8(ASSETS_ROOT) OPAL_UTF8("/gltf-Sample-Assets/Models")
void UIRenderer::RenderMeshConverterTool()
{

    static Opal::StringUtf8 s_base_path = Opal::Paths::Combine(nullptr, GLTF_SAMPLE_ASSETS, u8"DamagedHelmet", u8"glTF").GetValue();
    static Opal::StringUtf8 s_selected_file_path = Opal::Paths::Combine(nullptr, s_base_path, u8"DamagedHelmet.gltf").GetValue();
    static Opal::StringUtf8 s_selected_output_directory = s_base_path;
    static Opal::StringUtf8 s_scene_file_path = Opal::Paths::Combine(nullptr, s_base_path, u8"DamagedHelmet.rndrscene").GetValue();
    static Opal::StringUtf8 s_mesh_file_path = Opal::Paths::Combine(nullptr, s_base_path, u8"DamagedHelmet.rndrmesh").GetValue();
    static Opal::StringUtf8 s_material_file_path = Opal::Paths::Combine(nullptr, s_base_path, u8"DamagedHelmet.rndrmat").GetValue();

    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f));

    ImGui::Begin("Mesh Converter Tool", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if (ImGui::Button("Select file to convert..."))
    {
        s_selected_file_path = OpenFileDialog();
    }
    ImGui::Text("Selected file: %s", !s_selected_file_path.IsEmpty() ? s_selected_file_path.GetDataAs<c>() : "None");

    if (ImGui::Button("Select directory for the output"))
    {
        s_selected_output_directory = OpenFolderDialog();
        const Opal::StringUtf8 parent = Opal::Paths::GetParentPath(s_selected_file_path).GetValue();
        const Opal::StringUtf8 stem = Opal::Paths::GetStem(s_selected_file_path).GetValue();
        s_scene_file_path = Opal::Paths::Combine(nullptr, s_selected_output_directory, stem + u8".rndrscene").GetValue();
        s_mesh_file_path = Opal::Paths::Combine(nullptr, s_selected_output_directory, stem + u8".rndrmesh").GetValue();
        s_material_file_path = Opal::Paths::Combine(nullptr, s_selected_output_directory, stem + u8".rndrmat").GetValue();
    }
    ImGui::Text("Output scene file: %s", !s_scene_file_path.IsEmpty() ? s_scene_file_path.GetDataAs<c>() : "None");
    ImGui::Text("Output mesh file: %s", !s_mesh_file_path.IsEmpty() ? s_mesh_file_path.GetDataAs<c>() : "None");
    ImGui::Text("Output material file: %s", !s_material_file_path.IsEmpty() ? s_material_file_path.GetDataAs<c>() : "None");

    MeshAttributesToLoad attributes_to_load = MeshAttributesToLoad::LoadPositions;
    static bool s_should_load_normals = true;
    static bool s_should_load_uvs = true;
    static Opal::StringUtf8 s_status = u8"Idle";
    ImGui::Checkbox("Use Normals", &s_should_load_normals);
    ImGui::Checkbox("Use Uvs", &s_should_load_uvs);
    if (s_should_load_normals)
    {
        attributes_to_load |= MeshAttributesToLoad::LoadNormals;
    }
    if (s_should_load_uvs)
    {
        attributes_to_load |= MeshAttributesToLoad::LoadUvs;
    }
    if (ImGui::Button("Convert"))
    {
        ProcessScene(s_selected_file_path, s_scene_file_path, s_mesh_file_path, s_material_file_path, attributes_to_load, s_status);
    }
    ImGui::Text("Status: %s", s_status.GetData());
    ImGui::End();
}
#undef GLTF_SAMPLE_ASSETS

void UIRenderer::RenderComputeBrdfLutTool()
{
    ImGui::SetNextWindowPos(ImVec2(10.0f, 250.0f));

    ImGui::Begin("Compute BRDF LUT Tool", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    static Opal::StringUtf8 s_selected_file_path;
    if (ImGui::Button("Select output path..."))
    {
        s_selected_file_path = OpenFolderDialog();
    }
    Opal::StringUtf8 output_file_path = !s_selected_file_path.IsEmpty()
                                            ? (Opal::Paths::Combine(nullptr, s_selected_file_path, OPAL_UTF8("brdflut.ktx")).GetValue())
                                            : OPAL_UTF8("None");
    ImGui::Text("Output file: %s", output_file_path.GetDataAs<c>());
    static Opal::StringUtf8 s_status = OPAL_UTF8("Idle");
    if (ImGui::Button("Compute BRDF"))
    {
        if (s_selected_file_path.IsEmpty())
        {
            s_status = OPAL_UTF8("No output path selected!");
        }
        else
        {
            ComputeBrdfLut(output_file_path, s_status);
        }
    }
    ImGui::Text("Status: %s", s_status.GetDataAs<c>());
    ImGui::End();
}

void UIRenderer::RenderComputeEnvironmentMapTool()
{
    ImGui::SetNextWindowPos(ImVec2(10.0f, 400.0f));

    ImGui::Begin("Compute Environment Map Tool", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    static Opal::StringUtf8 s_selected_file = OPAL_UTF8("None");
    static Opal::StringUtf8 s_output_file = OPAL_UTF8("None");
    if (ImGui::Button("Select input environment map..."))
    {
        s_selected_file = OpenFileDialog();
        const Opal::StringUtf8 selected_directory = Opal::Paths::GetParentPath(s_selected_file).GetValue();
        const Opal::StringUtf8 selected_file_name = Opal::Paths::GetStem(s_selected_file).GetValue();
        const Opal::StringUtf8 selected_file_extension = Opal::Paths::GetExtension(s_selected_file).GetValue();
        s_output_file =
            Opal::Paths::Combine(nullptr, selected_directory, selected_file_name, OPAL_UTF8("_irradiance"), selected_file_extension)
                .GetValue();
    }

    ImGui::Text("Input file: %s", s_selected_file.GetDataAs<c>());
    ImGui::Text("Output file: %s", s_output_file.GetDataAs<c>());

    static Opal::StringUtf8 s_status = OPAL_UTF8("Idle");
    if (ImGui::Button("Convolve"))
    {
        if (s_output_file.IsEmpty())
        {
            s_status = OPAL_UTF8("No output path selected!");
        }
        else
        {
            ComputeEnvironmentMap(s_selected_file, s_output_file, s_status);
        }
    }
    ImGui::Text("Status: %s", s_status.GetDataAs<c>());
    ImGui::End();
}

void UIRenderer::ProcessScene(const Opal::StringUtf8& in_mesh_path, const Opal::StringUtf8& out_scene_path,
                              const Opal::StringUtf8& out_mesh_path, const Opal::StringUtf8& out_material_path,
                              MeshAttributesToLoad attributes_to_load, Opal::StringUtf8& out_status)
{
    constexpr uint32_t k_ai_process_flags = aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                                            aiProcess_LimitBoneWeights | aiProcess_SplitLargeMeshes | aiProcess_ImproveCacheLocality |
                                            aiProcess_RemoveRedundantMaterials | aiProcess_FindDegenerates | aiProcess_FindInvalidData |
                                            aiProcess_GenUVCoords;

    const aiScene* ai_scene = aiImportFile(in_mesh_path.GetDataAs<c>(), k_ai_process_flags);
    if (ai_scene == nullptr || !ai_scene->HasMeshes())
    {
        RNDR_LOG_ERROR("Failed to load mesh from file with error: %s", aiGetErrorString());
        out_status = OPAL_UTF8("Failed");
        return;
    }

    SceneDescription scene_desc;
    if (!AssimpHelpers::ReadSceneDescription(scene_desc, *ai_scene))
    {
        RNDR_LOG_ERROR("Failed to load scene description from file: %s", in_mesh_path.GetDataAs<c>());
        out_status = OPAL_UTF8("Failed");
        return;
    }

    MeshData mesh_data;
    if (!AssimpHelpers::ReadMeshData(mesh_data, *ai_scene, attributes_to_load))
    {
        RNDR_LOG_ERROR("Failed to load mesh data from file: %s", in_mesh_path.GetDataAs<c>());
        out_status = OPAL_UTF8("Failed");
        return;
    }

    Opal::Array<MaterialDescription> materials(ai_scene->mNumMaterials);
    Opal::Array<Opal::StringUtf8> texture_paths;
    Opal::Array<Opal::StringUtf8> opacity_maps;
    for (uint32_t i = 0; i < ai_scene->mNumMaterials; i++)
    {
        if (!AssimpHelpers::ReadMaterialDescription(materials[i], texture_paths, opacity_maps, *ai_scene->mMaterials[i]))
        {
            RNDR_LOG_ERROR("Failed to read material description from file: %s", in_mesh_path.GetDataAs<c>());
            out_status = OPAL_UTF8("Failed");
            return;
        }
    }
    aiReleaseImport(ai_scene);

    const Opal::StringUtf8 base_path = Opal::Paths::GetParentPath(in_mesh_path).GetValue();
    const Opal::StringUtf8 out_base_path = Opal::Paths::GetParentPath(out_mesh_path).GetValue();
    if (!Material::ConvertAndDownscaleTextures(materials, base_path, texture_paths, opacity_maps, out_base_path))
    {
        RNDR_LOG_ERROR("Failed to convert and downscale textures!");
        out_status = OPAL_UTF8("Failed");
        return;
    }

    for (const Opal::StringUtf8& texture_path : texture_paths)
    {
        RNDR_LOG_INFO("Texture path: %s", texture_path.GetDataAs<c>());
    }

    if (!Material::WriteData(materials, texture_paths, out_material_path))
    {
        RNDR_LOG_ERROR("Failed to write material data to file: %s", out_material_path.GetDataAs<c>());
        out_status = OPAL_UTF8("Failed");
        return;
    }

    if (!Mesh::WriteData(mesh_data, out_mesh_path))
    {
        RNDR_LOG_ERROR("Failed to write mesh data to file: %s", out_mesh_path.GetDataAs<c>());
        out_status = OPAL_UTF8("Failed");
        return;
    }

    if (!Scene::WriteSceneDescription(scene_desc, out_scene_path))
    {
        RNDR_LOG_ERROR("Failed to write scene description to file: %s", out_scene_path.GetDataAs<c>());
        out_status = OPAL_UTF8("Failed");
        return;
    }

    out_status = OPAL_UTF8("Success");
}

void UIRenderer::ComputeBrdfLut(const Opal::StringUtf8& output_path, Opal::StringUtf8& status)
{
    RNDR_UNUSED(output_path);
    m_desc.graphics_context->Bind(m_brdf_lut_buffer, 0);
    m_desc.graphics_context->Bind(m_brdf_lut_pipeline);
    if (!m_desc.graphics_context->DispatchCompute(m_brdf_lut_width, m_brdf_lut_height, 1))
    {
        status = OPAL_UTF8("Failed to dispatch compute shader!");
        return;
    }

    Opal::Array<float> read_data_storage(m_brdf_lut_width * m_brdf_lut_height * 2);
    Opal::Span<Opal::u8> read_data = Opal::AsWritableBytes(read_data_storage);
    if (m_desc.graphics_context->ReadBuffer(m_brdf_lut_buffer, read_data) == Rndr::ErrorCode::Success)
    {
        status = OPAL_UTF8("Failed to read buffer data!");
        return;
    }

    gli::texture lut_texture = gli::texture2d(gli::FORMAT_RG16_SFLOAT_PACK16, gli::extent2d(m_brdf_lut_width, m_brdf_lut_height), 1);

    for (int y = 0; y < m_brdf_lut_height; y++)
    {
        for (int x = 0; x < m_brdf_lut_width; x++)
        {
            const int ofs = y * m_brdf_lut_width + x;
            const gli::vec2 value(read_data_storage[ofs * 2 + 0], read_data_storage[ofs * 2 + 1]);
            const gli::texture::extent_type uv = {x, y, 0};
            lut_texture.store<glm::uint32>(uv, 0, 0, 0, gli::packHalf2x16(value));
        }
    }

    if (!gli::save_ktx(lut_texture, output_path.GetDataAs<c>()))
    {
        status = OPAL_UTF8("Failed to save BRDF LUT to file!");
        return;
    }

    status = OPAL_UTF8("BRDF LUT computed successfully!");
}

void UIRenderer::ComputeEnvironmentMap(const Opal::StringUtf8& input_path, const Opal::StringUtf8& output_path, Opal::StringUtf8& status)
{
    Rndr::Bitmap input_bitmap = Rndr::File::ReadEntireImage(input_path, Rndr::PixelFormat::R32G32B32_FLOAT);
    if (!input_bitmap.IsValid())
    {
        status = OPAL_UTF8("Failed to read input image!");
        return;
    }

    const int32_t input_width = input_bitmap.GetWidth();
    const int32_t input_height = input_bitmap.GetHeight();
    Rndr::Vector3f* input_data = reinterpret_cast<Rndr::Vector3f*>(input_bitmap.GetData());

    constexpr int32_t k_output_width = 256;
    constexpr int32_t k_output_height = 128;
    Opal::Array<Rndr::Vector3f> output_data(k_output_width * k_output_height);

    constexpr int32_t k_nb_monte_carlo_samples = 1024;
    if (!CubeMap::ConvolveDiffuse(input_data, input_width, input_height, k_output_width, k_output_height, output_data.GetData(),
                                        k_nb_monte_carlo_samples))
    {
        status = OPAL_UTF8("Failed to convolve input image!");
        return;
    }

    Rndr::Bitmap output_bitmap(k_output_width, k_output_height, 1, Rndr::PixelFormat::R32G32B32_FLOAT, Opal::AsWritableBytes(output_data));
    if (!Rndr::File::SaveImage(output_bitmap, output_path))
    {
        status = OPAL_UTF8("Failed to save output image!");
        return;
    }

    status = OPAL_UTF8("Environment map convolved successfully!");
}
