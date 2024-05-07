#pragma once

#include "opal/types.h"
#include "math/math.h"

using i8 = Opal::i8;
using i16 = Opal::i16;
using i32 = Opal::i32;
using i64 = Opal::i64;

using u8 = Opal::u8;
using u16 = Opal::u16;
using u32 = Opal::u32;
using u64 = Opal::u64;

using st = Opal::st;

using f32 = Opal::f32;
using f64 = Opal::f64;

using c = Opal::c;
using wc = Opal::wc;
using c8 = Opal::c8;
using c16 = Opal::c16;
using c32 = Opal::c32;

using Point2f = Math::Point2<float>;
using Point3f = Math::Point3<float>;
using Point4f = Math::Point4<float>;
using Vector2f = Math::Vector2<float>;
using Vector3f = Math::Vector3<float>;
using Vector4f = Math::Vector4<float>;
using Normal3f = Math::Normal3<float>;
using Matrix4x4f = Math::Matrix4x4<float>;
using Rotatorf = Math::Rotator<float>;
using Quaternionf = Math::Quaternion<float>;
using Bounds2f = Math::Bounds2<float>;
using Bounds3f = Math::Bounds3<float>;

using Point2i = Math::Point2<int32_t>;
using Vector2i = Math::Vector2<int32_t>;
