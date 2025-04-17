#pragma once

#include "opal/types.h"
#include "opal/math/matrix.h"
#include "opal/math/point2.h"
#include "opal/math/point3.h"
#include "opal/math/point4.h"
#include "opal/math/rotator.h"
#include "opal/math/bounds2.h"
#include "opal/math/bounds3.h"
#include "opal/math/quaternion.h"
#include "opal/math/dual-quaternion.h"

using i8 = Opal::i8;
using i16 = Opal::i16;
using i32 = Opal::i32;
using i64 = Opal::i64;

using u8 = Opal::u8;
using u16 = Opal::u16;
using u32 = Opal::u32;
using u64 = Opal::u64;

using size_t = Opal::size_t;

using f32 = Opal::f32;
using f64 = Opal::f64;

using char8 = Opal::char8;
using char16 = Opal::char16;
using uchar32 = Opal::uchar32;

using Point2f = Opal::Point2<f32>;
using Point3f = Opal::Point3<f32>;
using Point4f = Opal::Point4<f32>;
using Vector2f = Opal::Vector2<f32>;
using Vector3f = Opal::Vector3<f32>;
using Vector4f = Opal::Vector4<f32>;
using Normal3f = Opal::Normal3<f32>;
using Matrix4x4f = Opal::Matrix4x4<f32>;
using Rotatorf = Opal::Rotator<f32>;
using Quatf = Opal::Quaternion<f32>;
using DualQuatf = Opal::DualQuaternion<f32>;
using Bounds2f = Opal::Bounds2<f32>;
using Bounds3f = Opal::Bounds3<f32>;

using Point2i = Opal::Point2<i32>;
using Vector2i = Opal::Vector2<i32>;
