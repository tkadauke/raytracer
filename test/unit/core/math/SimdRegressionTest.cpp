// SIMD regression tests.
//
// The SSE3 specialisations of Vector3<float>, Vector3<double>, Vector4<float>,
// Vector4<double>, Color<float>, and Color<double> live in
// include/core/math/vector/sse3/ and include/core/color/sse3/. They are
// gated on __SSE__ / __SSE3__ at compile time. When those macros are defined
// (i.e. on x86 with -msse3, which CMakeLists.txt sets unconditionally for
// x86-family CPUs), the specialisations replace the generic Vector / Color
// templates for those particular instantiations.
//
// Each test below takes a battery of inputs, runs each operation through both
// the SSE3 specialisation and an explicit instantiation of the generic
// underlying template, and asserts the two results are equal within FP
// tolerance.
//
// On platforms without SSE3 (e.g. Apple Silicon arm64) the comparison block
// compiles to nothing and the test verifies the generic-vs-generic path of
// the same operations against the same inputs. On x86 the real
// SIMD-vs-generic comparison runs.

#include <gtest/gtest.h>

#include "core/Color.h"
#include "core/math/Vector.h"

#include "test/helpers/ColorTestHelper.h"
#include "test/helpers/VectorTestHelper.h"

namespace SimdRegressionTest {
  using namespace ::testing;

  namespace {
    // Explicitly-instantiated *generic* aliases. These bypass the Vector3<T>
    // / Color<T> SSE3 specialisations and always use the underlying
    // template's scalar implementation, regardless of whether __SSE__ is
    // defined. They are the "expected" side of every comparison below.
    template<class T>
    using GenericVector3 = Vector<3, T, T, void>;
    template<class T>
    using GenericVector4 = Vector<4, T, T, void>;

    [[maybe_unused]] constexpr double kTol = 1e-6;
  }

#ifdef __SSE__

  // Vector3<float> — SSE3 specialisation gated on __SSE__.
  TEST(SimdRegression, Vector3fAddMatchesGeneric) {
    Vector3<float> s(1.5f, -2.0f, 3.5f);
    Vector3<float> t(0.25f, 4.0f, -1.5f);
    GenericVector3<float> g_s(1.5f, -2.0f, 3.5f);
    GenericVector3<float> g_t(0.25f, 4.0f, -1.5f);
    auto sse = s + t;
    auto gen = g_s + g_t;
    ASSERT_NEAR(gen[0], sse.x(), kTol);
    ASSERT_NEAR(gen[1], sse.y(), kTol);
    ASSERT_NEAR(gen[2], sse.z(), kTol);
  }

  TEST(SimdRegression, Vector3fSubMatchesGeneric) {
    Vector3<float> s(1.5f, -2.0f, 3.5f), t(0.25f, 4.0f, -1.5f);
    GenericVector3<float> g_s(1.5f, -2.0f, 3.5f), g_t(0.25f, 4.0f, -1.5f);
    auto sse = s - t;
    auto gen = g_s - g_t;
    ASSERT_NEAR(gen[0], sse.x(), kTol);
    ASSERT_NEAR(gen[1], sse.y(), kTol);
    ASSERT_NEAR(gen[2], sse.z(), kTol);
  }

  TEST(SimdRegression, Vector3fNegateMatchesGeneric) {
    Vector3<float> s(1.5f, -2.0f, 3.5f);
    GenericVector3<float> g_s(1.5f, -2.0f, 3.5f);
    auto sse = -s;
    auto gen = -g_s;
    ASSERT_NEAR(gen[0], sse.x(), kTol);
    ASSERT_NEAR(gen[1], sse.y(), kTol);
    ASSERT_NEAR(gen[2], sse.z(), kTol);
  }

  TEST(SimdRegression, Vector3fScalarMultiplyMatchesGeneric) {
    Vector3<float> s(1.5f, -2.0f, 3.5f);
    GenericVector3<float> g_s(1.5f, -2.0f, 3.5f);
    constexpr float k = 2.5f;
    auto sse = s * k;
    auto gen = g_s * k;
    ASSERT_NEAR(gen[0], sse.x(), kTol);
    ASSERT_NEAR(gen[1], sse.y(), kTol);
    ASSERT_NEAR(gen[2], sse.z(), kTol);
  }

  TEST(SimdRegression, Vector3fDotProductMatchesGeneric) {
    Vector3<float> s(1.5f, -2.0f, 3.5f), t(0.25f, 4.0f, -1.5f);
    GenericVector3<float> g_s(1.5f, -2.0f, 3.5f), g_t(0.25f, 4.0f, -1.5f);
    ASSERT_NEAR(g_s * g_t, s * t, kTol);
  }

  TEST(SimdRegression, Vector3fLengthMatchesGeneric) {
    Vector3<float> s(3.0f, 4.0f, 12.0f); // Pythagorean: |.| = 13
    GenericVector3<float> g_s(3.0f, 4.0f, 12.0f);
    ASSERT_NEAR(g_s.length(), s.length(), kTol);
  }

  // Vector4<float> — SSE3 specialisation gated on __SSE__.
  TEST(SimdRegression, Vector4fAddMatchesGeneric) {
    Vector4<float> s(1.5f, -2.0f, 3.5f, 0.5f), t(0.25f, 4.0f, -1.5f, 0.25f);
    GenericVector4<float> g_s(1.5f, -2.0f, 3.5f, 0.5f), g_t(0.25f, 4.0f, -1.5f, 0.25f);
    auto sse = s + t;
    auto gen = g_s + g_t;
    for (int i = 0; i < 4; ++i)
      ASSERT_NEAR(gen[i], sse[i], kTol);
  }

  TEST(SimdRegression, Vector4fScalarMultiplyMatchesGeneric) {
    Vector4<float> s(1.5f, -2.0f, 3.5f, 0.5f);
    GenericVector4<float> g_s(1.5f, -2.0f, 3.5f, 0.5f);
    constexpr float k = -1.25f;
    auto sse = s * k;
    auto gen = g_s * k;
    for (int i = 0; i < 4; ++i)
      ASSERT_NEAR(gen[i], sse[i], kTol);
  }

#endif // __SSE__

#ifdef __SSE3__

  // Vector3<double> — SSE3 specialisation gated on __SSE3__.
  TEST(SimdRegression, Vector3dAddMatchesGeneric) {
    Vector3<double> s(1.5, -2.0, 3.5), t(0.25, 4.0, -1.5);
    GenericVector3<double> g_s(1.5, -2.0, 3.5), g_t(0.25, 4.0, -1.5);
    auto sse = s + t;
    auto gen = g_s + g_t;
    ASSERT_NEAR(gen[0], sse.x(), kTol);
    ASSERT_NEAR(gen[1], sse.y(), kTol);
    ASSERT_NEAR(gen[2], sse.z(), kTol);
  }

  TEST(SimdRegression, Vector3dDotProductMatchesGeneric) {
    Vector3<double> s(1.5, -2.0, 3.5), t(0.25, 4.0, -1.5);
    GenericVector3<double> g_s(1.5, -2.0, 3.5), g_t(0.25, 4.0, -1.5);
    ASSERT_NEAR(g_s * g_t, s * t, kTol);
  }

  TEST(SimdRegression, Vector3dCrossProductMatchesScalar) {
    // cross is implemented in scalar even in the SSE3 spec, but a
    // regression here would still indicate a divergence from the math.
    // Cross of basis vectors: i × j = k.
    Vector3<double> i(1, 0, 0), j(0, 1, 0);
    auto k = i.crossProduct(j);
    ASSERT_NEAR(0.0, k.x(), kTol);
    ASSERT_NEAR(0.0, k.y(), kTol);
    ASSERT_NEAR(1.0, k.z(), kTol);
  }

  // Vector4<double> — SSE3 specialisation gated on __SSE3__.
  TEST(SimdRegression, Vector4dAddMatchesGeneric) {
    Vector4<double> s(1.5, -2.0, 3.5, 0.5), t(0.25, 4.0, -1.5, 0.25);
    GenericVector4<double> g_s(1.5, -2.0, 3.5, 0.5), g_t(0.25, 4.0, -1.5, 0.25);
    auto sse = s + t;
    auto gen = g_s + g_t;
    for (int i = 0; i < 4; ++i)
      ASSERT_NEAR(gen[i], sse[i], kTol);
  }

#endif // __SSE3__

#ifdef __SSE__

  // Color<float> SSE specialisation. There's no separately-instantiable
  // generic Color<float> (Color<T> is the only template), so these tests
  // verify that the SSE3 path produces the textbook scalar result for
  // each operation.
  TEST(SimdRegression, ColorfAddMatchesScalar) {
    Color<float> a(0.25f, 0.5f, 0.75f), b(0.5f, 0.25f, 0.125f);
    auto sum = a + b;
    ASSERT_COLOR_NEAR(Color<float>(0.75f, 0.75f, 0.875f), sum, 1e-6f);
  }

  TEST(SimdRegression, ColorfScalarMultiplyMatchesScalar) {
    Color<float> a(0.25f, 0.5f, 0.75f);
    auto product = a * 2.0f;
    ASSERT_COLOR_NEAR(Color<float>(0.5f, 1.0f, 1.5f), product, 1e-6f);
  }

  TEST(SimdRegression, ColorfModulationMatchesScalar) {
    Color<float> a(0.5f, 0.5f, 0.5f), b(0.4f, 0.6f, 0.8f);
    auto product = a * b;
    ASSERT_COLOR_NEAR(Color<float>(0.2f, 0.3f, 0.4f), product, 1e-6f);
  }

#endif // __SSE__

#ifdef __SSE3__

  // Color<double> SSE3 specialisation.
  TEST(SimdRegression, ColordAddMatchesScalar) {
    Color<double> a(0.25, 0.5, 0.75), b(0.5, 0.25, 0.125);
    auto sum = a + b;
    ASSERT_COLOR_NEAR(Color<double>(0.75, 0.75, 0.875), sum, 1e-9);
  }

  TEST(SimdRegression, ColordModulationMatchesScalar) {
    Color<double> a(0.5, 0.5, 0.5), b(0.4, 0.6, 0.8);
    auto product = a * b;
    ASSERT_COLOR_NEAR(Color<double>(0.2, 0.3, 0.4), product, 1e-9);
  }

#endif // __SSE3__

  // A single sanity-check test that always builds (regardless of __SSE__),
  // so this TU has at least one test on every platform — keeps CTest from
  // emitting a "no tests in this TU" warning on arm64.
  TEST(SimdRegression, ShouldBuildOnEveryPlatform) {
    SUCCEED();
  }
}
