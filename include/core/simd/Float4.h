#pragma once

#include "core/SimdFeatures.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#if RAYTRACER_SIMD_SSE
#include <xmmintrin.h>
#endif

#if RAYTRACER_SIMD_NEON
#include <arm_neon.h>
#endif

namespace core::simd {
  struct ScalarBackend {};

#if RAYTRACER_SIMD_SSE
  struct SseBackend {};
#endif

#if RAYTRACER_SIMD_NEON
  struct NeonBackend {};
#endif

  template<class Backend>
  class Float4T;

  template<class Backend>
  class Mask4T;

  template<>
  class Float4T<ScalarBackend> {
  public:
    using Storage = std::array<float, 4>;

    Float4T() = default;
    explicit Float4T(Storage value)
        : m_value(value) {
    }

    [[nodiscard]] const Storage& value() const {
      return m_value;
    }

  private:
    Storage m_value{};
  };

  template<>
  class Mask4T<ScalarBackend> {
  public:
    using Storage = std::array<std::uint32_t, 4>;

    Mask4T() = default;
    explicit Mask4T(Storage value)
        : m_value(value) {
    }

    [[nodiscard]] const Storage& value() const {
      return m_value;
    }

  private:
    Storage m_value{};
  };

#if RAYTRACER_SIMD_SSE
  template<>
  class Float4T<SseBackend> {
  public:
    using Storage = __m128;

    Float4T() = default;
    explicit Float4T(__m128 value)
        : m_value(value) {
    }

    [[nodiscard]] __m128 value() const {
      return m_value;
    }

  private:
    __m128 m_value{};
  };

  template<>
  class Mask4T<SseBackend> {
  public:
    using Storage = __m128;

    Mask4T() = default;
    explicit Mask4T(__m128 value)
        : m_value(value) {
    }

    [[nodiscard]] __m128 value() const {
      return m_value;
    }

  private:
    __m128 m_value{};
  };
#endif

#if RAYTRACER_SIMD_NEON
  template<>
  class Float4T<NeonBackend> {
  public:
    using Storage = float32x4_t;

    Float4T() = default;
    explicit Float4T(float32x4_t value)
        : m_value(value) {
    }

    [[nodiscard]] float32x4_t value() const {
      return m_value;
    }

  private:
    float32x4_t m_value{};
  };

  template<>
  class Mask4T<NeonBackend> {
  public:
    using Storage = uint32x4_t;

    Mask4T() = default;
    explicit Mask4T(uint32x4_t value)
        : m_value(value) {
    }

    [[nodiscard]] uint32x4_t value() const {
      return m_value;
    }

  private:
    uint32x4_t m_value{};
  };
#endif

#if RAYTRACER_SIMD_SSE
  using NativeBackend = SseBackend;
#elif RAYTRACER_SIMD_NEON
  using NativeBackend = NeonBackend;
#else
  using NativeBackend = ScalarBackend;
#endif

  using Float4 = Float4T<NativeBackend>;
  using Mask4 = Mask4T<NativeBackend>;
  using ScalarFloat4 = Float4T<ScalarBackend>;
  using ScalarMask4 = Mask4T<ScalarBackend>;

  template<class Backend>
  [[nodiscard]] inline Float4T<Backend> load4(const float* values);

  template<class Backend>
  inline void store4(float* values, Float4T<Backend> value);

  template<class Backend>
  [[nodiscard]] inline Float4T<Backend> set1(float value);

  template<class Backend>
  [[nodiscard]] inline Float4T<Backend> zero();

  template<class Backend>
  [[nodiscard]] inline Float4T<Backend> min(Float4T<Backend> lhs, Float4T<Backend> rhs);

  template<class Backend>
  [[nodiscard]] inline Float4T<Backend> max(Float4T<Backend> lhs, Float4T<Backend> rhs);

  template<class Backend>
  [[nodiscard]] inline Float4T<Backend> sqrt(Float4T<Backend> value);

  template<class Backend>
  [[nodiscard]] inline Mask4T<Backend> cmpEq(Float4T<Backend> lhs, Float4T<Backend> rhs);

  template<class Backend>
  [[nodiscard]] inline Mask4T<Backend> cmpNe(Float4T<Backend> lhs, Float4T<Backend> rhs);

  template<class Backend>
  [[nodiscard]] inline Mask4T<Backend> cmpLt(Float4T<Backend> lhs, Float4T<Backend> rhs);

  template<class Backend>
  [[nodiscard]] inline Mask4T<Backend> cmpLe(Float4T<Backend> lhs, Float4T<Backend> rhs);

  template<class Backend>
  [[nodiscard]] inline Mask4T<Backend> cmpGt(Float4T<Backend> lhs, Float4T<Backend> rhs);

  template<class Backend>
  [[nodiscard]] inline Mask4T<Backend> cmpGe(Float4T<Backend> lhs, Float4T<Backend> rhs);

  template<class Backend>
  [[nodiscard]] inline Float4T<Backend> select(Mask4T<Backend> mask, Float4T<Backend> trueValue,
                                               Float4T<Backend> falseValue);

  template<class Backend>
  [[nodiscard]] inline Mask4T<Backend> maskAnd(Mask4T<Backend> lhs, Mask4T<Backend> rhs);

  template<class Backend>
  [[nodiscard]] inline Mask4T<Backend> maskOr(Mask4T<Backend> lhs, Mask4T<Backend> rhs);

  template<class Backend>
  [[nodiscard]] inline Mask4T<Backend> maskXor(Mask4T<Backend> lhs, Mask4T<Backend> rhs);

  template<class Backend>
  [[nodiscard]] inline Mask4T<Backend> maskAndNot(Mask4T<Backend> mask, Mask4T<Backend> value);

  template<class Backend>
  [[nodiscard]] inline Mask4T<Backend> maskNot(Mask4T<Backend> mask);

  template<class Backend>
  [[nodiscard]] inline int movemask(Mask4T<Backend> mask);

  template<class Backend>
  [[nodiscard]] inline Float4T<Backend> operator+(Float4T<Backend> lhs, Float4T<Backend> rhs);

  template<class Backend>
  [[nodiscard]] inline Float4T<Backend> operator-(Float4T<Backend> lhs, Float4T<Backend> rhs);

  template<class Backend>
  [[nodiscard]] inline Float4T<Backend> operator*(Float4T<Backend> lhs, Float4T<Backend> rhs);

  template<class Backend>
  [[nodiscard]] inline Float4T<Backend> operator/(Float4T<Backend> lhs, Float4T<Backend> rhs);

  namespace detail {
    [[nodiscard]] inline std::uint32_t maskLane(bool value) {
      return value ? 0xffffffffu : 0u;
    }
  }

  template<>
  [[nodiscard]] inline ScalarFloat4 load4<ScalarBackend>(const float* values) {
    return ScalarFloat4({values[0], values[1], values[2], values[3]});
  }

  template<>
  inline void store4<ScalarBackend>(float* values, ScalarFloat4 value) {
    const auto& lanes = value.value();
    for (std::size_t i = 0; i < lanes.size(); ++i)
      values[i] = lanes[i];
  }

  template<>
  [[nodiscard]] inline ScalarFloat4 set1<ScalarBackend>(float value) {
    return ScalarFloat4({value, value, value, value});
  }

  template<>
  [[nodiscard]] inline ScalarFloat4 zero<ScalarBackend>() {
    return set1<ScalarBackend>(0.0f);
  }

  template<>
  [[nodiscard]] inline ScalarFloat4 min<ScalarBackend>(ScalarFloat4 lhs, ScalarFloat4 rhs) {
    const auto& a = lhs.value();
    const auto& b = rhs.value();
    // Match _mm_min_ps unordered-lane behavior: choose rhs when comparison is false.
    return ScalarFloat4({a[0] < b[0] ? a[0] : b[0], a[1] < b[1] ? a[1] : b[1],
                         a[2] < b[2] ? a[2] : b[2], a[3] < b[3] ? a[3] : b[3]});
  }

  template<>
  [[nodiscard]] inline ScalarFloat4 max<ScalarBackend>(ScalarFloat4 lhs, ScalarFloat4 rhs) {
    const auto& a = lhs.value();
    const auto& b = rhs.value();
    // Match _mm_max_ps unordered-lane behavior: choose rhs when comparison is false.
    return ScalarFloat4({a[0] > b[0] ? a[0] : b[0], a[1] > b[1] ? a[1] : b[1],
                         a[2] > b[2] ? a[2] : b[2], a[3] > b[3] ? a[3] : b[3]});
  }

  template<>
  [[nodiscard]] inline ScalarFloat4 sqrt<ScalarBackend>(ScalarFloat4 value) {
    const auto& lanes = value.value();
    return ScalarFloat4(
      {std::sqrt(lanes[0]), std::sqrt(lanes[1]), std::sqrt(lanes[2]), std::sqrt(lanes[3])});
  }

#define RAYTRACER_SIMD_SCALAR_CMP(name, op)                                                        \
  template<>                                                                                       \
  [[nodiscard]] inline ScalarMask4 name<ScalarBackend>(ScalarFloat4 lhs, ScalarFloat4 rhs) {       \
    const auto& a = lhs.value();                                                                   \
    const auto& b = rhs.value();                                                                   \
    return ScalarMask4({detail::maskLane(a[0] op b[0]), detail::maskLane(a[1] op b[1]),            \
                        detail::maskLane(a[2] op b[2]), detail::maskLane(a[3] op b[3])});          \
  }

  RAYTRACER_SIMD_SCALAR_CMP(cmpEq, ==)
  RAYTRACER_SIMD_SCALAR_CMP(cmpNe, !=)
  RAYTRACER_SIMD_SCALAR_CMP(cmpLt, <)
  RAYTRACER_SIMD_SCALAR_CMP(cmpLe, <=)
  RAYTRACER_SIMD_SCALAR_CMP(cmpGt, >)
  RAYTRACER_SIMD_SCALAR_CMP(cmpGe, >=)

#undef RAYTRACER_SIMD_SCALAR_CMP

  template<>
  [[nodiscard]] inline ScalarFloat4 select<ScalarBackend>(ScalarMask4 mask, ScalarFloat4 trueValue,
                                                          ScalarFloat4 falseValue) {
    const auto& m = mask.value();
    const auto& t = trueValue.value();
    const auto& f = falseValue.value();
    return ScalarFloat4(
      {m[0] ? t[0] : f[0], m[1] ? t[1] : f[1], m[2] ? t[2] : f[2], m[3] ? t[3] : f[3]});
  }

  template<>
  [[nodiscard]] inline ScalarMask4 maskAnd<ScalarBackend>(ScalarMask4 lhs, ScalarMask4 rhs) {
    const auto& a = lhs.value();
    const auto& b = rhs.value();
    return ScalarMask4({a[0] & b[0], a[1] & b[1], a[2] & b[2], a[3] & b[3]});
  }

  template<>
  [[nodiscard]] inline ScalarMask4 maskOr<ScalarBackend>(ScalarMask4 lhs, ScalarMask4 rhs) {
    const auto& a = lhs.value();
    const auto& b = rhs.value();
    return ScalarMask4({a[0] | b[0], a[1] | b[1], a[2] | b[2], a[3] | b[3]});
  }

  template<>
  [[nodiscard]] inline ScalarMask4 maskXor<ScalarBackend>(ScalarMask4 lhs, ScalarMask4 rhs) {
    const auto& a = lhs.value();
    const auto& b = rhs.value();
    return ScalarMask4({a[0] ^ b[0], a[1] ^ b[1], a[2] ^ b[2], a[3] ^ b[3]});
  }

  template<>
  [[nodiscard]] inline ScalarMask4 maskAndNot<ScalarBackend>(ScalarMask4 mask, ScalarMask4 value) {
    const auto& m = mask.value();
    const auto& v = value.value();
    return ScalarMask4({(~m[0]) & v[0], (~m[1]) & v[1], (~m[2]) & v[2], (~m[3]) & v[3]});
  }

  template<>
  [[nodiscard]] inline ScalarMask4 maskNot<ScalarBackend>(ScalarMask4 mask) {
    const auto& lanes = mask.value();
    return ScalarMask4({~lanes[0], ~lanes[1], ~lanes[2], ~lanes[3]});
  }

  template<>
  [[nodiscard]] inline int movemask<ScalarBackend>(ScalarMask4 mask) {
    const auto& lanes = mask.value();
    return ((lanes[0] >> 31) & 0x1) | (((lanes[1] >> 31) & 0x1) << 1) |
           (((lanes[2] >> 31) & 0x1) << 2) | (((lanes[3] >> 31) & 0x1) << 3);
  }

#define RAYTRACER_SIMD_SCALAR_OP(op)                                                               \
  template<>                                                                                       \
  [[nodiscard]] inline ScalarFloat4 operator op<ScalarBackend>(ScalarFloat4 lhs,                   \
                                                               ScalarFloat4 rhs) {                 \
    const auto& a = lhs.value();                                                                   \
    const auto& b = rhs.value();                                                                   \
    return ScalarFloat4({a[0] op b[0], a[1] op b[1], a[2] op b[2], a[3] op b[3]});                 \
  }

  RAYTRACER_SIMD_SCALAR_OP(+)
  RAYTRACER_SIMD_SCALAR_OP(-)
  RAYTRACER_SIMD_SCALAR_OP(*)
  RAYTRACER_SIMD_SCALAR_OP(/)

#undef RAYTRACER_SIMD_SCALAR_OP

#if RAYTRACER_SIMD_SSE
  template<>
  [[nodiscard]] inline Float4T<SseBackend> load4<SseBackend>(const float* values) {
    return Float4T<SseBackend>(_mm_load_ps(values));
  }

  template<>
  inline void store4<SseBackend>(float* values, Float4T<SseBackend> value) {
    _mm_store_ps(values, value.value());
  }

  template<>
  [[nodiscard]] inline Float4T<SseBackend> set1<SseBackend>(float value) {
    return Float4T<SseBackend>(_mm_set1_ps(value));
  }

  template<>
  [[nodiscard]] inline Float4T<SseBackend> zero<SseBackend>() {
    return Float4T<SseBackend>(_mm_setzero_ps());
  }

  template<>
  [[nodiscard]] inline Float4T<SseBackend> min<SseBackend>(Float4T<SseBackend> lhs,
                                                           Float4T<SseBackend> rhs) {
    return Float4T<SseBackend>(_mm_min_ps(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Float4T<SseBackend> max<SseBackend>(Float4T<SseBackend> lhs,
                                                           Float4T<SseBackend> rhs) {
    return Float4T<SseBackend>(_mm_max_ps(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Float4T<SseBackend> sqrt<SseBackend>(Float4T<SseBackend> value) {
    return Float4T<SseBackend>(_mm_sqrt_ps(value.value()));
  }

  template<>
  [[nodiscard]] inline Mask4T<SseBackend> cmpEq<SseBackend>(Float4T<SseBackend> lhs,
                                                            Float4T<SseBackend> rhs) {
    return Mask4T<SseBackend>(_mm_cmpeq_ps(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Mask4T<SseBackend> cmpNe<SseBackend>(Float4T<SseBackend> lhs,
                                                            Float4T<SseBackend> rhs) {
    return Mask4T<SseBackend>(_mm_cmpneq_ps(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Mask4T<SseBackend> cmpLt<SseBackend>(Float4T<SseBackend> lhs,
                                                            Float4T<SseBackend> rhs) {
    return Mask4T<SseBackend>(_mm_cmplt_ps(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Mask4T<SseBackend> cmpLe<SseBackend>(Float4T<SseBackend> lhs,
                                                            Float4T<SseBackend> rhs) {
    return Mask4T<SseBackend>(_mm_cmple_ps(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Mask4T<SseBackend> cmpGt<SseBackend>(Float4T<SseBackend> lhs,
                                                            Float4T<SseBackend> rhs) {
    return Mask4T<SseBackend>(_mm_cmpgt_ps(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Mask4T<SseBackend> cmpGe<SseBackend>(Float4T<SseBackend> lhs,
                                                            Float4T<SseBackend> rhs) {
    return Mask4T<SseBackend>(_mm_cmpge_ps(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Float4T<SseBackend> select<SseBackend>(Mask4T<SseBackend> mask,
                                                              Float4T<SseBackend> trueValue,
                                                              Float4T<SseBackend> falseValue) {
    return Float4T<SseBackend>(_mm_or_ps(_mm_and_ps(mask.value(), trueValue.value()),
                                         _mm_andnot_ps(mask.value(), falseValue.value())));
  }

  template<>
  [[nodiscard]] inline Mask4T<SseBackend> maskAnd<SseBackend>(Mask4T<SseBackend> lhs,
                                                              Mask4T<SseBackend> rhs) {
    return Mask4T<SseBackend>(_mm_and_ps(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Mask4T<SseBackend> maskOr<SseBackend>(Mask4T<SseBackend> lhs,
                                                             Mask4T<SseBackend> rhs) {
    return Mask4T<SseBackend>(_mm_or_ps(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Mask4T<SseBackend> maskXor<SseBackend>(Mask4T<SseBackend> lhs,
                                                              Mask4T<SseBackend> rhs) {
    return Mask4T<SseBackend>(_mm_xor_ps(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Mask4T<SseBackend> maskAndNot<SseBackend>(Mask4T<SseBackend> mask,
                                                                 Mask4T<SseBackend> value) {
    return Mask4T<SseBackend>(_mm_andnot_ps(mask.value(), value.value()));
  }

  template<>
  [[nodiscard]] inline Mask4T<SseBackend> maskNot<SseBackend>(Mask4T<SseBackend> mask) {
    const __m128 allBits = _mm_cmpeq_ps(_mm_setzero_ps(), _mm_setzero_ps());
    return maskAndNot<SseBackend>(mask, Mask4T<SseBackend>(allBits));
  }

  template<>
  [[nodiscard]] inline int movemask<SseBackend>(Mask4T<SseBackend> mask) {
    return _mm_movemask_ps(mask.value());
  }

  template<>
  [[nodiscard]] inline Float4T<SseBackend> operator+
    <SseBackend>(Float4T<SseBackend> lhs, Float4T<SseBackend> rhs) {
    return Float4T<SseBackend>(_mm_add_ps(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Float4T<SseBackend> operator-
    <SseBackend>(Float4T<SseBackend> lhs, Float4T<SseBackend> rhs) {
    return Float4T<SseBackend>(_mm_sub_ps(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Float4T<SseBackend> operator*
    <SseBackend>(Float4T<SseBackend> lhs, Float4T<SseBackend> rhs) {
    return Float4T<SseBackend>(_mm_mul_ps(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Float4T<SseBackend> operator/
    <SseBackend>(Float4T<SseBackend> lhs, Float4T<SseBackend> rhs) {
    return Float4T<SseBackend>(_mm_div_ps(lhs.value(), rhs.value()));
  }
#endif

#if RAYTRACER_SIMD_NEON
  template<>
  [[nodiscard]] inline Float4T<NeonBackend> load4<NeonBackend>(const float* values) {
    return Float4T<NeonBackend>(vld1q_f32(values));
  }

  template<>
  inline void store4<NeonBackend>(float* values, Float4T<NeonBackend> value) {
    vst1q_f32(values, value.value());
  }

  template<>
  [[nodiscard]] inline Float4T<NeonBackend> set1<NeonBackend>(float value) {
    return Float4T<NeonBackend>(vdupq_n_f32(value));
  }

  template<>
  [[nodiscard]] inline Float4T<NeonBackend> zero<NeonBackend>() {
    return set1<NeonBackend>(0.0f);
  }

  template<>
  [[nodiscard]] inline Float4T<NeonBackend> min<NeonBackend>(Float4T<NeonBackend> lhs,
                                                             Float4T<NeonBackend> rhs) {
    return Float4T<NeonBackend>(
      vbslq_f32(vcltq_f32(lhs.value(), rhs.value()), lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Float4T<NeonBackend> max<NeonBackend>(Float4T<NeonBackend> lhs,
                                                             Float4T<NeonBackend> rhs) {
    return Float4T<NeonBackend>(
      vbslq_f32(vcgtq_f32(lhs.value(), rhs.value()), lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Float4T<NeonBackend> sqrt<NeonBackend>(Float4T<NeonBackend> value) {
#if defined(__aarch64__)
    return Float4T<NeonBackend>(vsqrtq_f32(value.value()));
#else
    alignas(16) float lanes[4];
    store4<NeonBackend>(lanes, value);
    const float roots[4] = {std::sqrt(lanes[0]), std::sqrt(lanes[1]), std::sqrt(lanes[2]),
                            std::sqrt(lanes[3])};
    return load4<NeonBackend>(roots);
#endif
  }

  template<>
  [[nodiscard]] inline Mask4T<NeonBackend> cmpEq<NeonBackend>(Float4T<NeonBackend> lhs,
                                                              Float4T<NeonBackend> rhs) {
    return Mask4T<NeonBackend>(vceqq_f32(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Mask4T<NeonBackend> cmpNe<NeonBackend>(Float4T<NeonBackend> lhs,
                                                              Float4T<NeonBackend> rhs) {
    return maskNot<NeonBackend>(cmpEq<NeonBackend>(lhs, rhs));
  }

  template<>
  [[nodiscard]] inline Mask4T<NeonBackend> cmpLt<NeonBackend>(Float4T<NeonBackend> lhs,
                                                              Float4T<NeonBackend> rhs) {
    return Mask4T<NeonBackend>(vcltq_f32(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Mask4T<NeonBackend> cmpLe<NeonBackend>(Float4T<NeonBackend> lhs,
                                                              Float4T<NeonBackend> rhs) {
    return Mask4T<NeonBackend>(vcleq_f32(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Mask4T<NeonBackend> cmpGt<NeonBackend>(Float4T<NeonBackend> lhs,
                                                              Float4T<NeonBackend> rhs) {
    return Mask4T<NeonBackend>(vcgtq_f32(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Mask4T<NeonBackend> cmpGe<NeonBackend>(Float4T<NeonBackend> lhs,
                                                              Float4T<NeonBackend> rhs) {
    return Mask4T<NeonBackend>(vcgeq_f32(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Float4T<NeonBackend> select<NeonBackend>(Mask4T<NeonBackend> mask,
                                                                Float4T<NeonBackend> trueValue,
                                                                Float4T<NeonBackend> falseValue) {
    return Float4T<NeonBackend>(vbslq_f32(mask.value(), trueValue.value(), falseValue.value()));
  }

  template<>
  [[nodiscard]] inline Mask4T<NeonBackend> maskAnd<NeonBackend>(Mask4T<NeonBackend> lhs,
                                                                Mask4T<NeonBackend> rhs) {
    return Mask4T<NeonBackend>(vandq_u32(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Mask4T<NeonBackend> maskOr<NeonBackend>(Mask4T<NeonBackend> lhs,
                                                               Mask4T<NeonBackend> rhs) {
    return Mask4T<NeonBackend>(vorrq_u32(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Mask4T<NeonBackend> maskXor<NeonBackend>(Mask4T<NeonBackend> lhs,
                                                                Mask4T<NeonBackend> rhs) {
    return Mask4T<NeonBackend>(veorq_u32(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Mask4T<NeonBackend> maskAndNot<NeonBackend>(Mask4T<NeonBackend> mask,
                                                                   Mask4T<NeonBackend> value) {
    return Mask4T<NeonBackend>(vbicq_u32(value.value(), mask.value()));
  }

  template<>
  [[nodiscard]] inline Mask4T<NeonBackend> maskNot<NeonBackend>(Mask4T<NeonBackend> mask) {
    return Mask4T<NeonBackend>(vmvnq_u32(mask.value()));
  }

  template<>
  [[nodiscard]] inline int movemask<NeonBackend>(Mask4T<NeonBackend> mask) {
    alignas(16) std::uint32_t lanes[4];
    vst1q_u32(lanes, mask.value());
    return ((lanes[0] >> 31) & 0x1) | (((lanes[1] >> 31) & 0x1) << 1) |
           (((lanes[2] >> 31) & 0x1) << 2) | (((lanes[3] >> 31) & 0x1) << 3);
  }

  template<>
  [[nodiscard]] inline Float4T<NeonBackend> operator+
    <NeonBackend>(Float4T<NeonBackend> lhs, Float4T<NeonBackend> rhs) {
    return Float4T<NeonBackend>(vaddq_f32(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Float4T<NeonBackend> operator-
    <NeonBackend>(Float4T<NeonBackend> lhs, Float4T<NeonBackend> rhs) {
    return Float4T<NeonBackend>(vsubq_f32(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Float4T<NeonBackend> operator*
    <NeonBackend>(Float4T<NeonBackend> lhs, Float4T<NeonBackend> rhs) {
    return Float4T<NeonBackend>(vmulq_f32(lhs.value(), rhs.value()));
  }

  template<>
  [[nodiscard]] inline Float4T<NeonBackend> operator/
    <NeonBackend>(Float4T<NeonBackend> lhs, Float4T<NeonBackend> rhs) {
#if defined(__aarch64__)
    return Float4T<NeonBackend>(vdivq_f32(lhs.value(), rhs.value()));
#else
    alignas(16) float a[4];
    alignas(16) float b[4];
    store4<NeonBackend>(a, lhs);
    store4<NeonBackend>(b, rhs);
    const float quotients[4] = {a[0] / b[0], a[1] / b[1], a[2] / b[2], a[3] / b[3]};
    return load4<NeonBackend>(quotients);
#endif
  }
#endif

  [[nodiscard]] inline Float4 load4(const float* values) {
    return load4<NativeBackend>(values);
  }

  inline void store4(float* values, Float4 value) {
    store4<NativeBackend>(values, value);
  }

  [[nodiscard]] inline Float4 set1(float value) {
    return set1<NativeBackend>(value);
  }

  [[nodiscard]] inline Float4 zero() {
    return zero<NativeBackend>();
  }
}
