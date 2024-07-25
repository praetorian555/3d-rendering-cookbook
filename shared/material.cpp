#include "material.h"

#include <execution>

#include "stb_image.h"
#include "stb_image_resize2.h"
#include "stb_image_write.h"

#include "opal/container/hash-map.h"
#include "opal/container/string-hash.h"
#include "opal/paths.h"

#include "rndr/core/file.h"

namespace
{
Opal::StringUtf8 ConvertTexture(const Opal::StringUtf8& texture_path, const Opal::StringUtf8& base_path,
                                Opal::HashMap<Opal::StringUtf8, uint64_t, Opal::Hash<Opal::StringUtf8>>& albedo_texture_path_to_opacity_texture_index,
                                const Opal::Array<Opal::StringUtf8>& opacity_textures);
bool SetupMaterial(MaterialDescription& in_out_material, Opal::Array<Rndr::Image>& out_textures,
                   const Rndr::GraphicsContext& graphics_context, const Opal::Array<Opal::StringUtf8>& in_texture_paths);
Rndr::Image LoadTexture(const Rndr::GraphicsContext& graphics_context, const Opal::StringUtf8& texture_path);
}  // namespace

bool Material::ConvertAndDownscaleTextures(const Opal::Array<MaterialDescription>& materials, const Opal::StringUtf8& base_path,
                                           Opal::Array<Opal::StringUtf8>& texture_paths,
                                           const Opal::Array<Opal::StringUtf8>& opacity_textures)
{
    Opal::HashMap<Opal::StringUtf8, uint64_t, Opal::Hash<Opal::StringUtf8>> albedo_map_path_to_opacity_map_index(texture_paths.GetSize());
    for (const MaterialDescription& mat_desc : materials)
    {
        if (mat_desc.opacity_texture != k_invalid_image_id && mat_desc.albedo_texture != k_invalid_image_id)
        {
            albedo_map_path_to_opacity_map_index[texture_paths[static_cast<size_t>(mat_desc.albedo_texture)]] = mat_desc.opacity_texture;
        }
    }

    auto converter = [&](const Opal::StringUtf8& s)
    { return ConvertTexture(s, base_path, albedo_map_path_to_opacity_map_index, opacity_textures); };

    std::transform(std::execution::par, texture_paths.begin(), texture_paths.end(), texture_paths.begin(), converter);
    return true;
}

bool Material::WriteData(const Opal::Array<MaterialDescription>& materials, const Opal::Array<Opal::StringUtf8>& texture_paths,
                         const Opal::StringUtf8& file_path)
{
    Opal::StringLocale file_path_locale;
    file_path_locale.Resize(300);
    const Opal::ErrorCode err = Opal::Transcode(file_path, file_path_locale);
    if (err != Opal::ErrorCode::Success)
    {
        RNDR_LOG_ERROR("Failed to transcode file path!");
        return false;
    }
    Rndr::FileHandler f(file_path_locale.GetData(), "wb");
    if (!f.IsValid())
    {
        RNDR_LOG_ERROR("Failed to open file %s", file_path_locale.GetData());
        return false;
    }

    size_t texture_paths_count = texture_paths.GetSize();
    f.Write(&texture_paths_count, sizeof(texture_paths_count), 1);
    for (const Opal::StringUtf8& texture_path : texture_paths)
    {
        RNDR_ASSERT(!texture_path.IsEmpty());
        size_t texture_path_length = texture_path.GetSize();
        f.Write(&texture_path_length, sizeof(texture_path_length), 1);
        f.Write(texture_path.GetData(), sizeof(texture_path[0]), texture_path_length);
    }

    const size_t materials_count = materials.GetSize();
    RNDR_ASSERT(materials_count > 0);
    f.Write(&materials_count, sizeof(materials_count), 1);
    f.Write(materials.GetData(), sizeof(materials[0]), materials_count);

    return true;
}

bool Material::ReadDataLoadTextures(Opal::Array<MaterialDescription>& out_materials, Opal::Array<Rndr::Image>& out_textures,
                                    const Opal::StringUtf8& file_path, const Rndr::GraphicsContext& graphics_context)
{
    Opal::StringLocale file_path_locale;
    file_path_locale.Resize(300);
    const Opal::ErrorCode err = Opal::Transcode(file_path, file_path_locale);
    if (err != Opal::ErrorCode::Success)
    {
        RNDR_LOG_ERROR("Failed to transcode file path!");
        return false;
    }
    Rndr::FileHandler f(file_path_locale.GetData(), "rb");
    if (!f.IsValid())
    {
        RNDR_LOG_ERROR("Failed to open file %s", file_path_locale.GetData());
        return false;
    }

    size_t texture_paths_count = 0;
    if (!f.Read(&texture_paths_count, sizeof(texture_paths_count), 1))
    {
        RNDR_LOG_ERROR("Failed to read texture paths count!");
        return false;
    }

    auto base_path_result = Opal::Paths::GetParentPath(file_path);
    RNDR_ASSERT(base_path_result.HasValue());
    Opal::StringUtf8 base_path = Opal::Move(base_path_result.GetValue());
    Opal::Array<Opal::StringUtf8> texture_paths(texture_paths_count);
    for (uint32_t i = 0; i < texture_paths_count; ++i)
    {
        size_t texture_path_length = 0;
        if (!f.Read(&texture_path_length, sizeof(texture_path_length), 1))
        {
            RNDR_LOG_ERROR("Failed to read texture path length!");
            return false;
        }
        RNDR_ASSERT(texture_path_length > 0);

        texture_paths[i].Resize(texture_path_length);
        if (!f.Read(texture_paths[i].GetData(), sizeof(texture_paths[i][0]), texture_path_length))
        {
            RNDR_LOG_ERROR("Failed to read texture path!");
            return false;
        }

        auto combine_result = Opal::Paths::Combine(nullptr, base_path, texture_paths[i]);
        RNDR_ASSERT(combine_result.HasValue());
        texture_paths[i] = Opal::Move(combine_result.GetValue());
    }

    size_t materials_count = 0;
    if (!f.Read(&materials_count, sizeof(materials_count), 1))
    {
        RNDR_LOG_ERROR("Failed to read materials count!");
        return false;
    }
    RNDR_ASSERT(materials_count > 0);

    out_materials.Resize(materials_count);
    if (!f.Read(out_materials.GetData(), sizeof(out_materials[0]), materials_count))
    {
        RNDR_LOG_ERROR("Failed to read materials!");
        return false;
    }

    for (MaterialDescription& material : out_materials)
    {
        if (!SetupMaterial(material, out_textures, graphics_context, texture_paths))
        {
            RNDR_LOG_ERROR("Failed to setup material!");
            return false;
        }
    }

    return true;
}

namespace
{
bool SetupMaterial(MaterialDescription& in_out_material, Opal::Array<Rndr::Image>& out_textures,
                   const Rndr::GraphicsContext& graphics_context, const Opal::Array<Opal::StringUtf8>& in_texture_paths)
{
    if (in_out_material.albedo_texture != k_invalid_image_id)
    {
        const Opal::StringUtf8& albedo_map_path = in_texture_paths[static_cast<size_t>(in_out_material.albedo_texture)];
        Rndr::Image albedo_map = LoadTexture(graphics_context, albedo_map_path);
        if (!albedo_map.IsValid())
        {
            RNDR_LOG_ERROR("Failed to load albedo map: %s", reinterpret_cast<const c*>(albedo_map_path.GetData()));
            return false;
        }
        in_out_material.albedo_texture = albedo_map.GetBindlessHandle();
        out_textures.PushBack(std::move(albedo_map));
    }
    else
    {
        in_out_material.albedo_texture = 0;
    }
    if (in_out_material.metallic_roughness_texture != k_invalid_image_id)
    {
        const Opal::StringUtf8& metallic_roughness_map_path =
            in_texture_paths[static_cast<size_t>(in_out_material.metallic_roughness_texture)];
        Rndr::Image metallic_roughness_map = LoadTexture(graphics_context, metallic_roughness_map_path);
        if (!metallic_roughness_map.IsValid())
        {
            RNDR_LOG_ERROR("Failed to load metallic roughness map: %s", metallic_roughness_map_path.GetData());
            return false;
        }
        in_out_material.metallic_roughness_texture = metallic_roughness_map.GetBindlessHandle();
        out_textures.PushBack(std::move(metallic_roughness_map));
    }
    else
    {
        in_out_material.metallic_roughness_texture = 0;
    }
    if (in_out_material.normal_texture != k_invalid_image_id)
    {
        const Opal::StringUtf8& normal_map_path = in_texture_paths[static_cast<size_t>(in_out_material.normal_texture)];
        Rndr::Image normal_map = LoadTexture(graphics_context, normal_map_path);
        if (!normal_map.IsValid())
        {
            RNDR_LOG_ERROR("Failed to load normal map: %s", normal_map_path.GetData());
            return false;
        }
        in_out_material.normal_texture = normal_map.GetBindlessHandle();
        out_textures.PushBack(std::move(normal_map));
    }
    else
    {
        in_out_material.normal_texture = 0;
    }
    if (in_out_material.ambient_occlusion_texture != k_invalid_image_id)
    {
        const Opal::StringUtf8& ambient_occlusion_map_path =
            in_texture_paths[static_cast<size_t>(in_out_material.ambient_occlusion_texture)];
        Rndr::Image ambient_occlusion_map = LoadTexture(graphics_context, ambient_occlusion_map_path);
        if (!ambient_occlusion_map.IsValid())
        {
            RNDR_LOG_ERROR("Failed to load ambient occlusion map: %s", ambient_occlusion_map_path.GetData());
            return false;
        }
        in_out_material.ambient_occlusion_texture = ambient_occlusion_map.GetBindlessHandle();
        out_textures.PushBack(std::move(ambient_occlusion_map));
    }
    else
    {
        in_out_material.ambient_occlusion_texture = 0;
    }
    if (in_out_material.emissive_texture != k_invalid_image_id)
    {
        const Opal::StringUtf8& emissive_map_path = in_texture_paths[static_cast<size_t>(in_out_material.emissive_texture)];
        Rndr::Image emissive_map = LoadTexture(graphics_context, emissive_map_path);
        if (!emissive_map.IsValid())
        {
            RNDR_LOG_ERROR("Failed to load emissive map: %s", emissive_map_path.GetData());
            return false;
        }
        in_out_material.emissive_texture = emissive_map.GetBindlessHandle();
        out_textures.PushBack(std::move(emissive_map));
    }
    else
    {
        in_out_material.emissive_texture = 0;
    }

    in_out_material.opacity_texture = 0;

    return true;
}

Opal::StringUtf8 ConvertTexture(const Opal::StringUtf8& texture_path, const Opal::StringUtf8& base_path,
                                Opal::HashMap<Opal::StringUtf8, uint64_t, Opal::Hash<Opal::StringUtf8>>& albedo_texture_path_to_opacity_texture_index,
                                const Opal::Array<Opal::StringUtf8>& opacity_textures)
{
    constexpr int32_t k_max_new_width = 512;
    constexpr int32_t k_max_new_height = 512;

    auto src_file_result = Opal::Paths::Combine(nullptr, base_path, texture_path);
    RNDR_ASSERT(src_file_result.HasValue());
    const Opal::StringUtf8 src_file = Opal::Move(src_file_result.GetValue());

    auto relative_src_path_result = Opal::Paths::GetParentPath(texture_path);
    RNDR_ASSERT(relative_src_path_result.HasValue());
    const Opal::StringUtf8 relative_src_parent_path = Opal::Move(relative_src_path_result.GetValue());
    relative_src_path_result = Opal::Paths::GetStem(texture_path);
    RNDR_ASSERT(relative_src_path_result.HasValue());
    const Opal::StringUtf8 relative_src_stem = Opal::Move(relative_src_path_result.GetValue());

    auto src_path_result = Opal::Paths::GetParentPath(src_file);
    RNDR_ASSERT(src_path_result.HasValue());
    const Opal::StringUtf8 src_parent_path = Opal::Move(src_path_result.GetValue());
    src_path_result = Opal::Paths::GetStem(src_file);
    RNDR_ASSERT(src_path_result.HasValue());
    const Opal::StringUtf8 src_stem = Opal::Move(src_path_result.GetValue());

    const Opal::StringUtf8 relative_dst_file = Opal::Paths::Combine(nullptr, relative_src_parent_path, relative_src_stem, u8"_rescaled.png").GetValue();
    const Opal::StringUtf8 dst_file = Opal::Paths::Combine(nullptr, src_parent_path, src_stem, u8"_rescaled.png").GetValue();

    const char* src_file_raw = reinterpret_cast<const c*>(src_file.GetData());
    const char* dst_file_raw = reinterpret_cast<const c*>(dst_file.GetData());

    RNDR_LOG_DEBUG("ConvertTexture: %s -> %s", src_file_raw, dst_file_raw);

    // load this image
    int32_t src_width = 0;
    int32_t src_height = 0;
    int32_t src_channels = 0;
    stbi_uc* src_pixels = stbi_load(src_file_raw, &src_width, &src_height, &src_channels, STBI_rgb_alpha);
    uint8_t* src_data = src_pixels;
    src_channels = STBI_rgb_alpha;

    Opal::Array<u8> empty_image(k_max_new_width * k_max_new_height * 4);
    if (src_data == nullptr)
    {
        RNDR_LOG_DEBUG("ConvertTexture: Failed to load [%s] texture", src_file_raw);
        src_width = k_max_new_width;
        src_height = k_max_new_height;
        src_channels = STBI_rgb_alpha;
        src_data = empty_image.GetData();
    }

    // Check if there is an opacity map for this texture and if there is put the opacity values into the alpha channel
    // of this image.
    if (albedo_texture_path_to_opacity_texture_index.contains(texture_path))
    {
        const uint64_t opacity_map_index = albedo_texture_path_to_opacity_texture_index[texture_path];
        const Opal::StringUtf8 opacity_map_file = Opal::Paths::Combine(nullptr, base_path, opacity_textures[opacity_map_index]).GetValue();
        const char* opacity_map_file_raw = reinterpret_cast<const c*>(opacity_map_file.GetData());
        int32_t opacity_width = 0;
        int32_t opacity_height = 0;
        stbi_uc* opacity_pixels = stbi_load(opacity_map_file_raw, &opacity_width, &opacity_height, nullptr, 1);
        if (opacity_pixels == nullptr)
        {
            RNDR_LOG_WARNING("ConvertTexture: Failed to load opacity map [%s] for [%s] texture", opacity_map_file_raw,
                             src_file_raw);
        }
        if (opacity_width != src_width || opacity_height != src_height)
        {
            RNDR_LOG_WARNING("ConvertTexture: Opacity map [%s] has different size than [%s] texture", opacity_map_file_raw,
                             src_file_raw);
        }

        // store the opacity mask in the alpha component of this image
        if (opacity_pixels != nullptr && opacity_width == src_width && opacity_height == src_height)
        {
            for (int y = 0; y != opacity_height; y++)
            {
                for (int x = 0; x != opacity_width; x++)
                {
                    src_data[(y * opacity_width + x) * src_channels + 3] = opacity_pixels[y * opacity_width + x];
                }
            }
        }
        else
        {
            RNDR_LOG_WARNING("ConvertTexture: Skipping opacity map [%s] for [%s] texture", opacity_map_file_raw, src_file_raw);
        }

        stbi_image_free(opacity_pixels);
    }

    const uint32_t dst_image_size = src_width * src_height * src_channels;
    Opal::Array<uint8_t> dst_data(dst_image_size);
    uint8_t* dst = dst_data.GetData();

    const int dst_width = std::min(src_width, k_max_new_width);
    const int dst_height = std::min(src_height, k_max_new_height);

    if (stbir_resize_uint8_linear(src_data, src_width, src_height, 0, dst, dst_width, dst_height, 0,
                                  static_cast<stbir_pixel_layout>(src_channels)) == nullptr)
    {
        RNDR_LOG_ERROR("ConvertTexture: Failed to resize [%s] texture", src_file_raw);
        goto cleanup;
    }

    if (stbi_write_png(dst_file_raw, dst_width, dst_height, src_channels, dst, 0) == 0)
    {
        RNDR_LOG_ERROR("ConvertTexture: Failed to write [%s] texture", dst_file_raw);
        goto cleanup;
    }

cleanup:
    if (src_pixels != nullptr)
    {
        stbi_image_free(src_pixels);
    }

    return relative_dst_file;
}

Rndr::Image LoadTexture(const Rndr::GraphicsContext& graphics_context, const Opal::StringUtf8& texture_path)
{
    constexpr bool k_flip_vertically = true;
    Rndr::Bitmap bitmap = Rndr::File::ReadEntireImage(texture_path, Rndr::PixelFormat::R8G8B8A8_UNORM, k_flip_vertically);
    if (!bitmap.IsValid())
    {
        RNDR_LOG_ERROR("Failed to load texture from file: %s", texture_path.GetData());
        return {};
    }
    const Rndr::ImageDesc image_desc{.width = bitmap.GetWidth(),
                                     .height = bitmap.GetHeight(),
                                     .array_size = 1,
                                     .type = Rndr::ImageType::Image2D,
                                     .pixel_format = bitmap.GetPixelFormat(),
                                     .use_mips = true,
                                     .is_bindless = true,
                                     .sampler = {.max_anisotropy = 16.0f, .border_color = Rndr::Colors::k_white}};
    const Opal::Span<const u8> bitmap_data{bitmap.GetData(), bitmap.GetSize3D()};
    return {graphics_context, image_desc, bitmap_data};
}

}  // namespace
