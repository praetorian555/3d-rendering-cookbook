#include <filesystem>

#include <imgui.h>

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <gli/gli.hpp>
#include <gli/load_ktx.hpp>
#include <gli/texture2d.hpp>

#include "rndr/core/containers/scope-ptr.h"
#include "rndr/core/containers/string.h"
#include "rndr/core/file.h"
#include "rndr/core/render-api.h"
#include "rndr/core/renderer-base.h"
#include "rndr/core/window.h"

class UIRenderer : public Rndr::RendererBase
{
public:
    UIRenderer(const Rndr::String& name, Rndr::Window& window, const Rndr::RendererBaseDesc& desc);
    ~UIRenderer();

    bool Render() override;

private:
    void RenderMeshConverterTool();
    void RenderComputeBrdfLutTool();
    void RenderComputeEnvironmentMapTool();

    void ProcessScene(const Rndr::String& in_mesh_path, const Rndr::String& out_scene_path, const Rndr::String& out_mesh_path,
                      const Rndr::String& out_material_path, Rndr::MeshAttributesToLoad attributes_to_load, Rndr::String& out_status);
    void ComputeBrdfLut(const Rndr::String& output_path, Rndr::String& status);
    void ComputeEnvironmentMap(const Rndr::String& input_path, const Rndr::String& output_path, Rndr::String& status);

    Rndr::Buffer m_brdf_lut_buffer;
    Rndr::Shader m_brdf_lut_shader;
    Rndr::Pipeline m_brdf_lut_pipeline;
    int32_t m_brdf_lut_width = 256;
    int32_t m_brdf_lut_height = 256;
};

void Run();
Rndr::String OpenFileDialog();
Rndr::String OpenFolderDialog();

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
    const Rndr::ScopePtr<Rndr::RendererBase> clear_renderer =
        RNDR_MAKE_SCOPED(Rndr::ClearRenderer, "Clear", renderer_desc, Rndr::Vector4f(0.0f, 0.0f, 0.0f, 1.0f));
    const Rndr::ScopePtr<Rndr::RendererBase> ui_renderer = RNDR_MAKE_SCOPED(UIRenderer, "UI", window, renderer_desc);
    const Rndr::ScopePtr<Rndr::RendererBase> present_renderer = RNDR_MAKE_SCOPED(Rndr::PresentRenderer, "Present", renderer_desc);
    renderer_manager.AddRenderer(clear_renderer.get());
    renderer_manager.AddRenderer(ui_renderer.get());
    renderer_manager.AddRenderer(present_renderer.get());

    while (!window.IsClosed())
    {
        window.ProcessEvents();
        renderer_manager.Render();
    }
}

UIRenderer::UIRenderer(const Rndr::String& name, Rndr::Window& window, const Rndr::RendererBaseDesc& desc) : RendererBase(name, desc)
{
    Rndr::ImGuiWrapper::Init(window, *desc.graphics_context, {.display_demo_window = false});

    const uint32_t buffer_size = m_brdf_lut_width * m_brdf_lut_height * sizeof(float) * 2;
    m_brdf_lut_buffer =
        Rndr::Buffer(desc.graphics_context,
                     Rndr::BufferDesc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = buffer_size});
    RNDR_ASSERT(m_brdf_lut_buffer.IsValid());
    const Rndr::String shader_source = Rndr::File::ReadShader(ASSETS_ROOT, "compute-brdf.glsl");
    m_brdf_lut_shader = Rndr::Shader(desc.graphics_context, Rndr::ShaderDesc{.type = Rndr::ShaderType::Compute, .source = shader_source});
    RNDR_ASSERT(m_brdf_lut_shader.IsValid());
    m_brdf_lut_pipeline = Rndr::Pipeline(desc.graphics_context, Rndr::PipelineDesc{.compute_shader = &m_brdf_lut_shader});
    RNDR_ASSERT(m_brdf_lut_pipeline.IsValid());
}

UIRenderer::~UIRenderer()
{
    Rndr::ImGuiWrapper::Destroy();
}

bool UIRenderer::Render()
{
    Rndr::ImGuiWrapper::StartFrame();
    RenderMeshConverterTool();
    RenderComputeBrdfLutTool();
    RenderComputeEnvironmentMapTool();
    Rndr::ImGuiWrapper::EndFrame();
    return true;
}

#define GLTF_SAMPLE_ASSETS ASSETS_ROOT "/Samples/gltf-Sample-Assets/Models/"
void UIRenderer::RenderMeshConverterTool()
{
    static Rndr::String s_selected_file_path = GLTF_SAMPLE_ASSETS "DamagedHelmet/glTF/DamagedHelmet.gltf";
    static Rndr::String s_scene_file_path = GLTF_SAMPLE_ASSETS "DamagedHelmet/glTF/DamagedHelmet.rndrscene";
    static Rndr::String s_mesh_file_path = GLTF_SAMPLE_ASSETS "DamagedHelmet/glTF/DamagedHelmet.rndrmesh";
    static Rndr::String s_material_file_path = GLTF_SAMPLE_ASSETS "DamagedHelmet/glTF/DamagedHelmet.rndrmat";

    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f));

    ImGui::Begin("Mesh Converter Tool", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if (ImGui::Button("Select file to convert..."))
    {
        s_selected_file_path = OpenFileDialog();
        const std::filesystem::path path(s_selected_file_path);
        s_scene_file_path = path.parent_path().string() + "/" + path.stem().string() + ".rndrscene";
        s_mesh_file_path = path.parent_path().string() + "/" + path.stem().string() + ".rndrmesh";
        s_material_file_path = path.parent_path().string() + "/" + path.stem().string() + ".rndrmat";
    }
    ImGui::Text("Selected file: %s", !s_selected_file_path.empty() ? s_selected_file_path.c_str() : "None");
    ImGui::Text("Output scene file: %s", !s_scene_file_path.empty() ? s_scene_file_path.c_str() : "None");
    ImGui::Text("Output mesh file: %s", !s_mesh_file_path.empty() ? s_mesh_file_path.c_str() : "None");
    ImGui::Text("Output material file: %s", !s_material_file_path.empty() ? s_material_file_path.c_str() : "None");

    Rndr::MeshAttributesToLoad attributes_to_load = Rndr::MeshAttributesToLoad::LoadPositions;
    static bool s_should_load_normals = true;
    static bool s_should_load_uvs = true;
    static Rndr::String s_status = "Idle";
    ImGui::Checkbox("Use Normals", &s_should_load_normals);
    ImGui::Checkbox("Use Uvs", &s_should_load_uvs);
    if (s_should_load_normals)
    {
        attributes_to_load |= Rndr::MeshAttributesToLoad::LoadNormals;
    }
    if (s_should_load_uvs)
    {
        attributes_to_load |= Rndr::MeshAttributesToLoad::LoadUvs;
    }
    if (ImGui::Button("Convert"))
    {
        ProcessScene(s_selected_file_path, s_scene_file_path, s_mesh_file_path, s_material_file_path, attributes_to_load, s_status);
    }
    ImGui::Text("Status: %s", s_status.c_str());
    ImGui::End();
}

void UIRenderer::RenderComputeBrdfLutTool()
{
    ImGui::SetNextWindowPos(ImVec2(10.0f, 250.0f));

    ImGui::Begin("Compute BRDF LUT Tool", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    static Rndr::String s_selected_file_path;
    if (ImGui::Button("Select output path..."))
    {
        s_selected_file_path = OpenFolderDialog();
    }
    Rndr::String output_file_path = !s_selected_file_path.empty() ? (s_selected_file_path + "\\brdflut.ktx") : "None";
    ImGui::Text("Output file: %s", output_file_path.c_str());
    static Rndr::String s_status = "Idle";
    if (ImGui::Button("Compute BRDF"))
    {
        if (s_selected_file_path.empty())
        {
            s_status = "No output path selected!";
        }
        else
        {
            ComputeBrdfLut(output_file_path, s_status);
        }
    }
    ImGui::Text("Status: %s", s_status.c_str());
    ImGui::End();
}

void UIRenderer::RenderComputeEnvironmentMapTool()
{
    ImGui::SetNextWindowPos(ImVec2(10.0f, 400.0f));

    ImGui::Begin("Compute Environment Map Tool", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    static Rndr::String s_selected_file = "None";
    static Rndr::String s_output_file = "None";
    if (ImGui::Button("Select input environment map..."))
    {
        s_selected_file = OpenFileDialog();
        const std::filesystem::path path(s_selected_file);
        const Rndr::String selected_directory = path.parent_path().string();
        const Rndr::String selected_file_name = path.stem().string();
        const Rndr::String selected_file_extension = path.extension().string();
        s_output_file = selected_directory + "\\" + selected_file_name + "_irradiance" + selected_file_extension;
    }

    ImGui::Text("Input file: %s", s_selected_file.c_str());
    ImGui::Text("Output file: %s", s_output_file.c_str());

    static Rndr::String s_status = "Idle";
    if (ImGui::Button("Convolve"))
    {
        if (s_output_file.empty())
        {
            s_status = "No output path selected!";
        }
        else
        {
            ComputeEnvironmentMap(s_selected_file, s_output_file, s_status);
        }
    }
    ImGui::Text("Status: %s", s_status.c_str());
    ImGui::End();
}

void UIRenderer::ProcessScene(const Rndr::String& in_mesh_path, const Rndr::String& out_scene_path,  const Rndr::String& out_mesh_path, const Rndr::String& out_material_path,
                              Rndr::MeshAttributesToLoad attributes_to_load, Rndr::String& out_status)
{
    constexpr uint32_t k_ai_process_flags = aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                                            aiProcess_LimitBoneWeights | aiProcess_SplitLargeMeshes | aiProcess_ImproveCacheLocality |
                                            aiProcess_RemoveRedundantMaterials | aiProcess_FindDegenerates | aiProcess_FindInvalidData |
                                            aiProcess_GenUVCoords;

    const aiScene* ai_scene = aiImportFile(in_mesh_path.c_str(), k_ai_process_flags);
    if (ai_scene == nullptr || !ai_scene->HasMeshes())
    {
        RNDR_LOG_ERROR("Failed to load mesh from file with error: %s", aiGetErrorString());
        out_status = "Failed";
        return;
    }

    Rndr::SceneDescription scene_desc;
    if (!Rndr::AssimpHelpers::ReadSceneDescription(scene_desc, *ai_scene))
    {
        RNDR_LOG_ERROR("Failed to load scene description from file: %s", in_mesh_path.c_str());
        out_status = "Failed";
        return;
    }

    Rndr::MeshData mesh_data;
    if (!Rndr::AssimpHelpers::ReadMeshData(mesh_data, *ai_scene, attributes_to_load))
    {
        RNDR_LOG_ERROR("Failed to load mesh data from file: %s", in_mesh_path.c_str());
        out_status = "Failed";
        return;
    }

    Opal::Array<Rndr::MaterialDescription> materials(ai_scene->mNumMaterials);
    Opal::Array<Rndr::String> texture_paths;
    Opal::Array<Rndr::String> opacity_maps;
    for (uint32_t i = 0; i < ai_scene->mNumMaterials; i++)
    {
        if (!Rndr::AssimpHelpers::ReadMaterialDescription(materials[i], texture_paths, opacity_maps, *ai_scene->mMaterials[i]))
        {
            RNDR_LOG_ERROR("Failed to read material description from file: %s", in_mesh_path.c_str());
            out_status = "Failed";
            return;
        }
    }
    aiReleaseImport(ai_scene);

    const std::filesystem::path in_path(in_mesh_path);
    const Rndr::String base_path = in_path.parent_path().string();
    if (!Rndr::Material::ConvertAndDownscaleTextures(materials, base_path, texture_paths, opacity_maps))
    {
        RNDR_LOG_ERROR("Failed to convert and downscale textures!");
        out_status = "Failed";
        return;
    }

    for (const Rndr::String& texture_path : texture_paths)
    {
        RNDR_LOG_INFO("Texture path: %s", texture_path.c_str());
    }

    if (!Rndr::Material::WriteData(materials, texture_paths, out_material_path))
    {
        RNDR_LOG_ERROR("Failed to write material data to file: %s", out_material_path.c_str());
        out_status = "Failed";
        return;
    }

    if (!Rndr::Mesh::WriteData(mesh_data, out_mesh_path))
    {
        RNDR_LOG_ERROR("Failed to write mesh data to file: %s", out_mesh_path.c_str());
        out_status = "Failed";
        return;
    }

    if (!Rndr::Scene::WriteSceneDescription(scene_desc, out_scene_path))
    {
        RNDR_LOG_ERROR("Failed to write scene description to file: %s", out_scene_path.c_str());
        out_status = "Failed";
        return;
    }

    out_status = "Success";
}

void UIRenderer::ComputeBrdfLut(const Rndr::String& output_path, Rndr::String& status)
{
    RNDR_UNUSED(output_path);
    m_desc.graphics_context->Bind(m_brdf_lut_buffer, 0);
    m_desc.graphics_context->Bind(m_brdf_lut_pipeline);
    if (!m_desc.graphics_context->DispatchCompute(m_brdf_lut_width, m_brdf_lut_height, 1))
    {
        status = "Failed to dispatch compute shader!";
        return;
    }

    Opal::Array<float> read_data_storage(m_brdf_lut_width * m_brdf_lut_height * 2);
    Opal::Span<Opal::u8> read_data = Opal::AsWritableBytes(read_data_storage);
    if (m_desc.graphics_context->ReadBuffer(m_brdf_lut_buffer, read_data) == Rndr::ErrorCode::Success)
    {
        status = "Failed to read buffer data!";
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

    if (!gli::save_ktx(lut_texture, output_path))
    {
        status = "Failed to save BRDF LUT to file!";
        return;
    }

    status = "BRDF LUT computed successfully!";
}

void UIRenderer::ComputeEnvironmentMap(const Rndr::String& input_path, const Rndr::String& output_path, Rndr::String& status)
{
    Rndr::Bitmap input_bitmap = Rndr::File::ReadEntireImage(input_path, Rndr::PixelFormat::R32G32B32_FLOAT);
    if (!input_bitmap.IsValid())
    {
        status = "Failed to read input image!";
        return;
    }

    const int32_t input_width = input_bitmap.GetWidth();
    const int32_t input_height = input_bitmap.GetHeight();
    Rndr::Vector3f* input_data = reinterpret_cast<Rndr::Vector3f*>(input_bitmap.GetData());

    constexpr int32_t k_output_width = 256;
    constexpr int32_t k_output_height = 128;
    Opal::Array<Rndr::Vector3f> output_data(k_output_width * k_output_height);

    constexpr int32_t k_nb_monte_carlo_samples = 1024;
    if (!Rndr::CubeMap::ConvolveDiffuse(input_data, input_width, input_height, k_output_width, k_output_height, output_data.GetData(),
                                        k_nb_monte_carlo_samples))
    {
        status = "Failed to convolve input image!";
        return;
    }

    Rndr::Bitmap output_bitmap(k_output_width, k_output_height, 1, Rndr::PixelFormat::R32G32B32_FLOAT, Opal::AsWritableBytes(output_data));
    if (!Rndr::File::SaveImage(output_bitmap, output_path))
    {
        status = "Failed to save output image!";
        return;
    }

    status = "Environment map convolved successfully!";
}
