// Microbenchmarks for Quaternion. The class is currently minimal — these
// benchmarks pin the throughput of what exists so the planned expansion
// (SLERP, axis-angle, rotate-vector, etc.) doesn't silently regress the
// existing operations.

#include <benchmark/benchmark.h>

#include <iostream>

#include "core/math/Quaternion.h"

namespace {

  template<typename T>
  void bm_multiply(benchmark::State& state) {
    Quaternion<T> a(T(0.7071), T(0.7071), T(0), T(0));
    Quaternion<T> b(T(0.5), T(0), T(0.866), T(0));
    for (auto _ : state) {
      benchmark::DoNotOptimize(a);
      benchmark::DoNotOptimize(b);
      auto r = a * b;
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename T>
  void bm_length(benchmark::State& state) {
    Quaternion<T> q(T(0.7071), T(0.7071), T(0), T(0));
    for (auto _ : state) {
      benchmark::DoNotOptimize(q);
      T r = q.length();
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename T>
  void bm_normalize(benchmark::State& state) {
    Quaternion<T> q(T(0.5), T(0.5), T(0.5), T(0.5));
    for (auto _ : state) {
      benchmark::DoNotOptimize(q);
      auto r = q.normalized();
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename T>
  void bm_scalar_mul(benchmark::State& state) {
    Quaternion<T> q(T(0.5), T(0.5), T(0.5), T(0.5));
    T s(2);
    for (auto _ : state) {
      benchmark::DoNotOptimize(q);
      benchmark::DoNotOptimize(s);
      auto r = q * s;
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename T>
  void bm_conjugate(benchmark::State& state) {
    Quaternion<T> q(T(0.7071), T(0.7071), T(0), T(0));
    for (auto _ : state) {
      benchmark::DoNotOptimize(q);
      auto r = q.conjugate();
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename T>
  void bm_inverse(benchmark::State& state) {
    Quaternion<T> q(T(0.7071), T(0.7071), T(0), T(0));
    for (auto _ : state) {
      benchmark::DoNotOptimize(q);
      auto r = q.inverse();
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename T>
  void bm_dot(benchmark::State& state) {
    Quaternion<T> a(T(0.7071), T(0.7071), T(0), T(0));
    Quaternion<T> b(T(0.5), T(0), T(0.866), T(0));
    for (auto _ : state) {
      benchmark::DoNotOptimize(a);
      benchmark::DoNotOptimize(b);
      T r = a.dot(b);
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename T>
  void bm_length_squared(benchmark::State& state) {
    Quaternion<T> q(T(0.7071), T(0.7071), T(0), T(0));
    for (auto _ : state) {
      benchmark::DoNotOptimize(q);
      T r = q.lengthSquared();
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename T>
  void bm_rotate_vector(benchmark::State& state) {
    Quaternion<T> q(T(0.7071), T(0), T(0), T(0.7071));
    Vector3<T> v(T(1), T(0), T(0));
    for (auto _ : state) {
      benchmark::DoNotOptimize(q);
      benchmark::DoNotOptimize(v);
      auto r = q.rotate(v);
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename T>
  void bm_from_axis_angle(benchmark::State& state) {
    Vector3<T> axis(T(0), T(0), T(1));
    T angle(T(1.5707963));
    for (auto _ : state) {
      benchmark::DoNotOptimize(axis);
      benchmark::DoNotOptimize(angle);
      auto r = Quaternion<T>::fromAxisAngle(axis, angle);
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename T>
  void bm_from_euler_angles(benchmark::State& state) {
    T rx(T(0.3)), ry(T(0.5)), rz(T(0.7));
    for (auto _ : state) {
      benchmark::DoNotOptimize(rx);
      benchmark::DoNotOptimize(ry);
      benchmark::DoNotOptimize(rz);
      auto r = Quaternion<T>::fromEulerAngles(rx, ry, rz);
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename T>
  void bm_to_euler_angles(benchmark::State& state) {
    Quaternion<T> q(T(0.7071), T(0.7071), T(0), T(0));
    for (auto _ : state) {
      benchmark::DoNotOptimize(q);
      auto r = q.toEulerAngles();
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename T>
  void bm_to_matrix3(benchmark::State& state) {
    Quaternion<T> q(T(0.7071), T(0), T(0), T(0.7071));
    for (auto _ : state) {
      benchmark::DoNotOptimize(q);
      auto r = q.toMatrix3();
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename T>
  void bm_to_matrix4(benchmark::State& state) {
    Quaternion<T> q(T(0.7071), T(0), T(0), T(0.7071));
    for (auto _ : state) {
      benchmark::DoNotOptimize(q);
      auto r = q.toMatrix4();
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename T>
  void bm_nlerp(benchmark::State& state) {
    Quaternion<T> a(T(1), T(0), T(0), T(0));
    Quaternion<T> b(T(0.7071), T(0), T(0), T(0.7071));
    T t(T(0.5));
    for (auto _ : state) {
      benchmark::DoNotOptimize(a);
      benchmark::DoNotOptimize(b);
      benchmark::DoNotOptimize(t);
      auto r = Quaternion<T>::nlerp(a, b, t);
      benchmark::DoNotOptimize(r);
    }
  }

  template<typename T>
  void bm_slerp(benchmark::State& state) {
    Quaternion<T> a(T(1), T(0), T(0), T(0));
    Quaternion<T> b(T(0.7071), T(0), T(0), T(0.7071));
    T t(T(0.5));
    for (auto _ : state) {
      benchmark::DoNotOptimize(a);
      benchmark::DoNotOptimize(b);
      benchmark::DoNotOptimize(t);
      auto r = Quaternion<T>::slerp(a, b, t);
      benchmark::DoNotOptimize(r);
    }
  }

} // namespace

BENCHMARK(bm_multiply<float>);
BENCHMARK(bm_multiply<double>);
BENCHMARK(bm_length<float>);
BENCHMARK(bm_length<double>);
BENCHMARK(bm_normalize<float>);
BENCHMARK(bm_normalize<double>);
BENCHMARK(bm_scalar_mul<float>);
BENCHMARK(bm_scalar_mul<double>);
BENCHMARK(bm_conjugate<float>);
BENCHMARK(bm_conjugate<double>);
BENCHMARK(bm_inverse<float>);
BENCHMARK(bm_inverse<double>);
BENCHMARK(bm_dot<float>);
BENCHMARK(bm_dot<double>);
BENCHMARK(bm_length_squared<float>);
BENCHMARK(bm_length_squared<double>);
BENCHMARK(bm_rotate_vector<float>);
BENCHMARK(bm_rotate_vector<double>);
BENCHMARK(bm_from_axis_angle<float>);
BENCHMARK(bm_from_axis_angle<double>);
BENCHMARK(bm_from_euler_angles<float>);
BENCHMARK(bm_from_euler_angles<double>);
BENCHMARK(bm_to_euler_angles<float>);
BENCHMARK(bm_to_euler_angles<double>);
BENCHMARK(bm_to_matrix3<float>);
BENCHMARK(bm_to_matrix3<double>);
BENCHMARK(bm_to_matrix4<float>);
BENCHMARK(bm_to_matrix4<double>);
BENCHMARK(bm_nlerp<float>);
BENCHMARK(bm_nlerp<double>);
BENCHMARK(bm_slerp<float>);
BENCHMARK(bm_slerp<double>);
