#pragma once

#include "opal/container/dynamic-array.h"
#include "opal/container/string.h"

#include "rndr/enum-flags.h"
#include "rndr/math.h"
#include "rndr/render-api.h"

#include "types.h"

enum class MaterialFlags : u8
{
    NoFlags = 0,
    Transparent = 1 << 0,

};
RNDR_ENUM_CLASS_FLAGS(MaterialFlags)

/**
 * Represents id of an image on the GPU.
 */
using ImageId = u64;

/**
 * Invalid image id. This is not used on the GPU but rather as a placeholder for an invalid image id.
 */
constexpr ImageId k_invalid_image_id = 0xFFFFFFFFFFFFFFFF;

/**
 * Description of a single material. It contains information about the material's properties and textures. It does not contain the actual
 * texture data but handles to the textures on the GPU.
 *
 * Note that when material description is loaded from the Assimp material the textures are not loaded and we store only the indices of the
 * texture paths in the array of texture paths. The actual loading of the textures is done later when the material is loaded.
 */
struct MaterialDescription final
{
    // Used only if emissive map is missing.
    Rndr::Vector4f emissive_color = Rndr::Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
    // Used only if albedo map is missing.
    Rndr::Vector4f albedo_color = Rndr::Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
    // Roughness of the surface, for anisotropic materials use the x and y while for isotropic materials the same value will be stored in
    // x and y. Used only if metallic_roughness_map is missing.
    Rndr::Vector4f roughness = Rndr::Vector4f(1.0f, 1.0f, 0.0f, 0.0f);

    float transparency_factor = 1.0f;
    float alpha_test = 0.0f;
    // Used only if metallic_roughness_map is missing.
    float metallic_factor = 0.0f;

    MaterialFlags flags = MaterialFlags::NoFlags;

    ImageId ambient_occlusion_texture = k_invalid_image_id;
    // Emissive color.
    ImageId emissive_texture = k_invalid_image_id;
    // Base color is stored in the RGB channels and opacity in the A channel.
    ImageId albedo_texture = k_invalid_image_id;
    // Roughness is stored in the G channel and metallic in the B channel.
    ImageId metallic_roughness_texture = k_invalid_image_id;
    ImageId normal_texture = k_invalid_image_id;
    // Not used in a cooked material. In cooked material opacity data will be in the alpha channel of the albedo map.
    ImageId opacity_texture = k_invalid_image_id;
};

namespace Material
{

/**
 * Rescales textures to 512x512, puts opacity data into the alpha channel of the albedo map and saves the textures as pngs with '_rescaled'
 * name suffix.
 * @param materials List of materials that use specified textures. Used to determine which opacity textures correspond to which albedo
 * textures.
 * @param base_path Path to the directory where the textures are located.
 * @param texture_paths List of texture paths relative to the base path.
 * @param opacity_textures List of paths to opacity textures relative to the base path.
 * @return True if the textures were successfully converted and downscaled, false otherwise.
 */
bool ConvertAndDownscaleTextures(const Opal::DynamicArray<MaterialDescription>& materials, const Opal::StringUtf8& base_path,
                                 Opal::DynamicArray<Opal::StringUtf8>& texture_paths, const Opal::DynamicArray<Opal::StringUtf8>& opacity_textures,
                                 const Opal::StringUtf8& out_base_path);

/**
 * Writes the material data to a file.
 * @param materials List of material descriptions to write.
 * @param texture_paths List of texture paths to write.
 * @param file_path Path to the file.
 * @return True if the material data was written successfully, false otherwise.
 */
bool WriteData(const Opal::DynamicArray<MaterialDescription>& materials, const Opal::DynamicArray<Opal::StringUtf8>& texture_paths,
               const Opal::StringUtf8& file_path);

/**
 * Reads the material data from a file and loads textures to the GPU.
 * @param out_materials Destination material descriptions.
 * @param out_textures Destination textures.
 * @param file_path Path to the file.
 * @param graphics_context Graphics context used to load the textures to the GPU.
 * @return True if the material data was read successfully and the textures were loaded to the GPU, false otherwise.
 */
bool ReadDataLoadTextures(Opal::DynamicArray<MaterialDescription>& out_materials, Opal::DynamicArray<Rndr::Texture>& out_textures,
                          const Opal::StringUtf8& file_path, const Rndr::GraphicsContext& graphics_context);

}  // namespace Material
