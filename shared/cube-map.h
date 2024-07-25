#pragma once

#include "rndr/core/bitmap.h"
#include "rndr/core/math.h"

#include "types.h"

namespace CubeMap
{

/**
 * Given a equirectangular map, convert it to a vertical cross bitmap.
 * @param in_bitmap Input equirectangular map.
 * @param out_bitmap Output vertical cross bitmap.
 * @return Returns true if conversion was successful, false otherwise.
 * @note Resulting vertical cross bitmap will have the following layout:
 *   +----+----+----+
 *   |    | +Y |    |
 *   | -X | -Z | +X |
 *   |    | -Y |    |
 *   |    | +Z |    |
 *   +----+----+----+
 */
bool ConvertEquirectangularMapToVerticalCross(const Rndr::Bitmap& in_bitmap, Rndr::Bitmap& out_bitmap);

/**
 * Given a vertical cross bitmap, convert it to a bitmap containing cube map faces. The expect layout is the same one
 * that is produced by ConvertEquirectangularMapToVerticalCross.
 * @param in_bitmap Input vertical cross bitmap.
 * @param out_bitmap Output bitmap containing cube map faces.
 * @return Returns true if conversion was successful, false otherwise.
 */
bool ConvertVerticalCrossToCubeMapFaces(const Rndr::Bitmap& in_bitmap, Rndr::Bitmap& out_bitmap);

/**
 * Convolve an equirectangular environment map with the GGX distribution of gltf shading model.
 * @param in_data Input equirectangular environment map.
 * @param in_width Input equirectangular environment map width.
 * @param in_height Input equirectangular environment map height.
 * @param out_width Output environment map width.
 * @param out_height Output environment map height.
 * @param out_data Output environment map.
 * @param nb_monte_carlo_samples Number of monte carlo samples to use for the convolution.
 * @return Returns true if convolution was successful, false otherwise.
 */
bool ConvolveDiffuse(const Rndr::Vector3f* in_data, i32 in_width, i32 in_height, i32 out_width, i32 out_height, Rndr::Vector3f* out_data,
                     i32 nb_monte_carlo_samples);

}  // namespace CubeMap
