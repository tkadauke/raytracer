// Microbenchmarks for Matrix2/3/4 hot paths. The renderer's transform
// pipeline is dominated by Matrix4 * Matrix4 (composing transforms),
// Matrix4 * Vector4 (transforming points), Matrix4::inverted() (caching
// inverse transforms once per Instance), and Matrix4::transposed() (normal
// matrix). All four are currently scalar triple-loops — these benchmarks
// pin the baseline before any SIMD work.

#include <benchmark/benchmark.h>

#include "core/math/Angle.h"
#include "core/math/Matrix.h"
#include "core/math/Vector.h"

namespace {

template <typename T, int Dim, typename Mat>
void fillMatrix(Mat& m) {
  for (int r = 0; r < Dim; ++r) {
    for (int c = 0; c < Dim; ++c) {
      m.setCell(r, c, T(r * Dim + c + 1));
    }
  }
}

// A well-conditioned 4x4 transform-like matrix: rotation + translation.
template <typename T>
Matrix4<T> makeTransform() {
  Matrix4<T> m;
  m.setCell(0, 0, T(0.866)); m.setCell(0, 1, T(-0.5));  m.setCell(0, 2, T(0));     m.setCell(0, 3, T(1));
  m.setCell(1, 0, T(0.5));   m.setCell(1, 1, T(0.866)); m.setCell(1, 2, T(0));     m.setCell(1, 3, T(2));
  m.setCell(2, 0, T(0));     m.setCell(2, 1, T(0));     m.setCell(2, 2, T(1));     m.setCell(2, 3, T(3));
  m.setCell(3, 0, T(0));     m.setCell(3, 1, T(0));     m.setCell(3, 2, T(0));     m.setCell(3, 3, T(1));
  return m;
}

template <typename T, int Dim, typename Mat>
void bm_matmul(benchmark::State& state) {
  Mat a, b;
  fillMatrix<T, Dim>(a);
  fillMatrix<T, Dim>(b);
  for (auto _ : state) {
    benchmark::DoNotOptimize(a);
    benchmark::DoNotOptimize(b);
    auto r = a * b;
    benchmark::DoNotOptimize(r);
  }
}

template <typename T, int Dim, typename Mat, typename Vec>
void bm_mat_vec(benchmark::State& state) {
  Mat m;
  fillMatrix<T, Dim>(m);
  Vec v(1, 2, 3);
  for (auto _ : state) {
    benchmark::DoNotOptimize(m);
    benchmark::DoNotOptimize(v);
    auto r = m * v;
    benchmark::DoNotOptimize(r);
  }
}

template <typename T>
void bm_inverted4(benchmark::State& state) {
  auto m = makeTransform<T>();
  for (auto _ : state) {
    benchmark::DoNotOptimize(m);
    auto r = m.inverted();
    benchmark::DoNotOptimize(r);
  }
}

template <typename T>
void bm_determinant4(benchmark::State& state) {
  auto m = makeTransform<T>();
  for (auto _ : state) {
    benchmark::DoNotOptimize(m);
    auto r = m.determinant();
    benchmark::DoNotOptimize(r);
  }
}

template <typename T, int Dim, typename Mat>
void bm_transposed(benchmark::State& state) {
  Mat m;
  fillMatrix<T, Dim>(m);
  for (auto _ : state) {
    benchmark::DoNotOptimize(m);
    auto r = m.transposed();
    benchmark::DoNotOptimize(r);
  }
}

template <typename T>
void bm_transform_point(benchmark::State& state) {
  auto m = makeTransform<T>();
  Vector3<T> p(1, 2, 3);
  for (auto _ : state) {
    benchmark::DoNotOptimize(m);
    benchmark::DoNotOptimize(p);
    auto r = m.transformPoint(p);
    benchmark::DoNotOptimize(r);
  }
}

template <typename T>
void bm_transform_direction(benchmark::State& state) {
  auto m = makeTransform<T>();
  Vector3<T> d(1, 2, 3);
  for (auto _ : state) {
    benchmark::DoNotOptimize(m);
    benchmark::DoNotOptimize(d);
    auto r = m.transformDirection(d);
    benchmark::DoNotOptimize(r);
  }
}

// "Build a transform from scratch" — a realistic camera/instance pattern.
// Stresses the three rotation factories plus a scale plus a chain of
// composition. Catches regressions in the factory functions themselves
// as well as in matmul.
template <typename T>
void bm_build_transform3(benchmark::State& state) {
  T z = T(0.5), y = T(1.0), x = T(0.25), s = T(2);
  for (auto _ : state) {
    benchmark::DoNotOptimize(z);
    benchmark::DoNotOptimize(y);
    benchmark::DoNotOptimize(x);
    benchmark::DoNotOptimize(s);
    auto r = Matrix3<T>::rotateZ(Angle<T>::fromRadians(z)) *
             Matrix3<T>::rotateY(Angle<T>::fromRadians(y)) *
             Matrix3<T>::rotateX(Angle<T>::fromRadians(x)) *
             Matrix3<T>::scale(s);
    benchmark::DoNotOptimize(r);
  }
}

}  // namespace

BENCHMARK(bm_matmul<float, 4, Matrix4f>);
BENCHMARK(bm_matmul<double, 4, Matrix4d>);
BENCHMARK(bm_matmul<float, 3, Matrix3f>);
BENCHMARK(bm_matmul<double, 3, Matrix3d>);

BENCHMARK(bm_mat_vec<float, 4, Matrix4f, Vector4f>);
BENCHMARK(bm_mat_vec<double, 4, Matrix4d, Vector4d>);
BENCHMARK(bm_mat_vec<float, 3, Matrix3f, Vector3f>);
BENCHMARK(bm_mat_vec<double, 3, Matrix3d, Vector3d>);

BENCHMARK(bm_inverted4<float>);
BENCHMARK(bm_inverted4<double>);

BENCHMARK(bm_determinant4<float>);
BENCHMARK(bm_determinant4<double>);

BENCHMARK(bm_transposed<float, 4, Matrix4f>);
BENCHMARK(bm_transposed<double, 4, Matrix4d>);

BENCHMARK(bm_transform_point<float>);
BENCHMARK(bm_transform_point<double>);
BENCHMARK(bm_transform_direction<float>);
BENCHMARK(bm_transform_direction<double>);

BENCHMARK(bm_build_transform3<float>);
BENCHMARK(bm_build_transform3<double>);
