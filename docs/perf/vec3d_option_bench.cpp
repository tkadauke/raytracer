// Phase 2.3 decision benchmark: Vector3<double> SSE3 resolution.
// Compares three implementations head-to-head in a single binary:
//   Baseline: current SSE3 two-register approach (from Vector3d.h)
//   Option A: plain scalar (what the compiler sees without the specialization;
//             compiled with -O3 -mavx2 the compiler can autovectorize)
//   Option B: AVX2 single __m256d register
//
// Compile:
//   g++ -std=c++17 -O3 -msse3 -mavx2 -I../include \
//       -o bench_vec3d_options vec3d_option_bench.cpp
//
// All three structs are benchmarked in the same binary so scheduling noise is
// shared across them; only the final summary numbers matter.

#include <chrono>
#include <cmath>
#include <cstdio>
#include <immintrin.h>
#include <pmmintrin.h>

// ---------------------------------------------------------------------------
// Baseline: current SSE3 two-register implementation (copied from Vector3d.h)
// ---------------------------------------------------------------------------
struct Vec3dSSE3 {
  using Coordinate = double;

  union {
    double m_coordinates[3];
    __m128d m_vector[2];
  };

  Vec3dSSE3() {
    m_vector[0] = _mm_setzero_pd();
    m_vector[1] = _mm_setzero_pd();
  }
  Vec3dSSE3(double x, double y, double z) {
    m_vector[0] = _mm_set_pd(y, x);
    m_vector[1] = _mm_set_sd(z);
  }
  Vec3dSSE3(const __m128d& v0, const __m128d& v1) {
    m_vector[0] = v0;
    m_vector[1] = v1;
  }

  double x() const {
    return m_coordinates[0];
  }
  double y() const {
    return m_coordinates[1];
  }
  double z() const {
    return m_coordinates[2];
  }

  Vec3dSSE3 operator+(const Vec3dSSE3& o) const {
    return Vec3dSSE3(_mm_add_pd(m_vector[0], o.m_vector[0]),
                     _mm_add_sd(m_vector[1], o.m_vector[1]));
  }
  Vec3dSSE3 operator-(const Vec3dSSE3& o) const {
    return Vec3dSSE3(_mm_sub_pd(m_vector[0], o.m_vector[0]),
                     _mm_sub_sd(m_vector[1], o.m_vector[1]));
  }
  Vec3dSSE3 operator*(double f) const {
    __m128d v = _mm_set1_pd(f);
    return Vec3dSSE3(_mm_mul_pd(m_vector[0], v), _mm_mul_sd(m_vector[1], v));
  }
  double operator*(const Vec3dSSE3& o) const {
    // Type-punning (this is UB in C++17, but faithfully reproduced for baseline)
    typedef union {
      __m128d vec;
      double coord[2];
    } Half;
    Half first, second;
    first.vec = _mm_mul_pd(m_vector[0], o.m_vector[0]);
    second.vec = _mm_mul_sd(m_vector[1], o.m_vector[1]);
    return first.coord[0] + first.coord[1] + second.coord[0];
  }
  // Cross product falls back to scalar in the original
  Vec3dSSE3 crossProduct(const Vec3dSSE3& o) const {
    return Vec3dSSE3(y() * o.z() - z() * o.y(), z() * o.x() - x() * o.z(),
                     x() * o.y() - y() * o.x());
  }
  double squaredLength() const {
    return (*this) * (*this);
  }
  double length() const {
    return std::sqrt(squaredLength());
  }
  Vec3dSSE3 normalized() const {
    return (*this) * (1.0 / length());
  }
};

// ---------------------------------------------------------------------------
// Option C (partial): baseline with the UB dot product fixed via _mm_hadd_pd.
// The cross product remains scalar (same as baseline). This tests whether
// fixing the UB alone preserves the baseline dot-product advantage.
// ---------------------------------------------------------------------------
struct Vec3dFixedSSE3 {
  using Coordinate = double;

  union {
    double m_coordinates[3];
    __m128d m_vector[2];
  };

  Vec3dFixedSSE3() {
    m_vector[0] = _mm_setzero_pd();
    m_vector[1] = _mm_setzero_pd();
  }
  Vec3dFixedSSE3(double x, double y, double z) {
    m_vector[0] = _mm_set_pd(y, x);
    m_vector[1] = _mm_set_sd(z);
  }
  Vec3dFixedSSE3(const __m128d& v0, const __m128d& v1) {
    m_vector[0] = v0;
    m_vector[1] = v1;
  }

  double x() const {
    return m_coordinates[0];
  }
  double y() const {
    return m_coordinates[1];
  }
  double z() const {
    return m_coordinates[2];
  }

  Vec3dFixedSSE3 operator+(const Vec3dFixedSSE3& o) const {
    return Vec3dFixedSSE3(_mm_add_pd(m_vector[0], o.m_vector[0]),
                          _mm_add_sd(m_vector[1], o.m_vector[1]));
  }
  Vec3dFixedSSE3 operator-(const Vec3dFixedSSE3& o) const {
    return Vec3dFixedSSE3(_mm_sub_pd(m_vector[0], o.m_vector[0]),
                          _mm_sub_sd(m_vector[1], o.m_vector[1]));
  }
  Vec3dFixedSSE3 operator*(double f) const {
    __m128d v = _mm_set1_pd(f);
    return Vec3dFixedSSE3(_mm_mul_pd(m_vector[0], v), _mm_mul_sd(m_vector[1], v));
  }
  // Fixed: use _mm_hadd_pd (SSE3) instead of type-punning union.
  double operator*(const Vec3dFixedSSE3& o) const {
    __m128d m0 = _mm_mul_pd(m_vector[0], o.m_vector[0]); // [x*ox, y*oy]
    __m128d m1 = _mm_mul_sd(m_vector[1], o.m_vector[1]); // [z*oz, 0]
    // hadd: [x*ox+y*oy, z*oz+0]
    __m128d h = _mm_hadd_pd(m0, m1);
    // hadd: [x*ox+y*oy+z*oz, ...]
    h = _mm_hadd_pd(h, h);
    return _mm_cvtsd_f64(h);
  }
  Vec3dFixedSSE3 crossProduct(const Vec3dFixedSSE3& o) const {
    return Vec3dFixedSSE3(y() * o.z() - z() * o.y(), z() * o.x() - x() * o.z(),
                          x() * o.y() - y() * o.x());
  }
  double squaredLength() const {
    return (*this) * (*this);
  }
  double length() const {
    return std::sqrt(squaredLength());
  }
  Vec3dFixedSSE3 normalized() const {
    return (*this) * (1.0 / length());
  }
};

// ---------------------------------------------------------------------------
// Option A: plain scalar — what the compiler sees without any specialization.
// With -O3 -mavx2 the compiler can autovectorize this loop.
// ---------------------------------------------------------------------------
struct Vec3dScalar {
  using Coordinate = double;

  double m_c[3];

  Vec3dScalar()
      : m_c{0, 0, 0} {
  }
  Vec3dScalar(double x, double y, double z)
      : m_c{x, y, z} {
  }

  double x() const {
    return m_c[0];
  }
  double y() const {
    return m_c[1];
  }
  double z() const {
    return m_c[2];
  }

  Vec3dScalar operator+(const Vec3dScalar& o) const {
    Vec3dScalar r;
    for (int i = 0; i < 3; ++i)
      r.m_c[i] = m_c[i] + o.m_c[i];
    return r;
  }
  Vec3dScalar operator-(const Vec3dScalar& o) const {
    Vec3dScalar r;
    for (int i = 0; i < 3; ++i)
      r.m_c[i] = m_c[i] - o.m_c[i];
    return r;
  }
  Vec3dScalar operator*(double f) const {
    Vec3dScalar r;
    for (int i = 0; i < 3; ++i)
      r.m_c[i] = m_c[i] * f;
    return r;
  }
  double operator*(const Vec3dScalar& o) const {
    double s = 0;
    for (int i = 0; i < 3; ++i)
      s += m_c[i] * o.m_c[i];
    return s;
  }
  Vec3dScalar crossProduct(const Vec3dScalar& o) const {
    return Vec3dScalar(m_c[1] * o.m_c[2] - m_c[2] * o.m_c[1], m_c[2] * o.m_c[0] - m_c[0] * o.m_c[2],
                       m_c[0] * o.m_c[1] - m_c[1] * o.m_c[0]);
  }
  double squaredLength() const {
    return (*this) * (*this);
  }
  double length() const {
    return std::sqrt(squaredLength());
  }
  Vec3dScalar normalized() const {
    return (*this) * (1.0 / length());
  }
};

// ---------------------------------------------------------------------------
// Option B: AVX2 single __m256d register, stores (x, y, z, 0)
// ---------------------------------------------------------------------------
#ifdef __AVX2__
struct Vec3dAVX2 {
  using Coordinate = double;

  __m256d v;

  Vec3dAVX2()
      : v(_mm256_setzero_pd()) {
  }
  Vec3dAVX2(double x, double y, double z)
      : v(_mm256_set_pd(0.0, z, y, x)) {
  }
  explicit Vec3dAVX2(__m256d r)
      : v(r) {
  }

  double x() const {
    return _mm256_cvtsd_f64(v);
  }
  double y() const {
    return _mm_cvtsd_f64(
      _mm256_extractf128_pd(_mm256_permute4x64_pd(v, _MM_SHUFFLE(1, 1, 1, 1)), 0));
  }
  double z() const {
    return _mm_cvtsd_f64(_mm256_extractf128_pd(v, 1));
  }

  Vec3dAVX2 operator+(const Vec3dAVX2& o) const {
    return Vec3dAVX2(_mm256_add_pd(v, o.v));
  }
  Vec3dAVX2 operator-(const Vec3dAVX2& o) const {
    return Vec3dAVX2(_mm256_sub_pd(v, o.v));
  }
  Vec3dAVX2 operator*(double f) const {
    return Vec3dAVX2(_mm256_mul_pd(v, _mm256_set1_pd(f)));
  }
  double operator*(const Vec3dAVX2& o) const {
    __m256d mul = _mm256_mul_pd(v, o.v);
    // hadd pairs within each 128-bit lane: [x*ox+y*oy, x*ox+y*oy, z*oz+0, z*oz+0]
    __m256d h = _mm256_hadd_pd(mul, mul);
    __m128d lo = _mm256_castpd256_pd128(h);
    __m128d hi = _mm256_extractf128_pd(h, 1);
    return _mm_cvtsd_f64(_mm_add_sd(lo, hi));
  }
  Vec3dAVX2 crossProduct(const Vec3dAVX2& o) const {
    // a×b = (ay*bz - az*by, az*bx - ax*bz, ax*by - ay*bx, 0)
    __m256d a1 = _mm256_permute4x64_pd(v, _MM_SHUFFLE(3, 0, 2, 1));   // ay,az,ax,0
    __m256d b1 = _mm256_permute4x64_pd(o.v, _MM_SHUFFLE(3, 1, 0, 2)); // bz,bx,by,0
    __m256d a2 = _mm256_permute4x64_pd(v, _MM_SHUFFLE(3, 1, 0, 2));   // az,ax,ay,0
    __m256d b2 = _mm256_permute4x64_pd(o.v, _MM_SHUFFLE(3, 0, 2, 1)); // by,bz,bx,0
    return Vec3dAVX2(_mm256_sub_pd(_mm256_mul_pd(a1, b1), _mm256_mul_pd(a2, b2)));
  }
  double squaredLength() const {
    return (*this) * (*this);
  }
  double length() const {
    return std::sqrt(squaredLength());
  }
  Vec3dAVX2 normalized() const {
    return (*this) * (1.0 / length());
  }
};
#endif // __AVX2__

// ---------------------------------------------------------------------------
// Timing harness
// ---------------------------------------------------------------------------
using Clock = std::chrono::steady_clock;
volatile double g_sink = 0;

template<typename Fn>
double measure_ns(Fn&& fn, int warmup = 100000, int reps = 5000000) {
  fn(warmup);
  auto t0 = Clock::now();
  fn(reps);
  auto t1 = Clock::now();
  return std::chrono::duration<double, std::nano>(t1 - t0).count() / reps;
}

template<typename Vec>
double bench_dot() {
  return measure_ns([](int N) {
    Vec a(1.0, 2.0, 3.0), b(4.0, 5.0, 6.0);
    double acc = 0;
    for (int i = 0; i < N; ++i) {
      asm volatile("" : "+m"(a), "+m"(b));
      acc += a * b;
    }
    g_sink += acc;
  });
}

template<typename Vec>
double bench_add() {
  return measure_ns([](int N) {
    Vec a(1.0, 2.0, 3.0), b(4.0, 5.0, 6.0);
    double acc = 0;
    for (int i = 0; i < N; ++i) {
      asm volatile("" : "+m"(a), "+m"(b));
      auto r = a + b;
      acc += r.x();
    }
    g_sink += acc;
  });
}

template<typename Vec>
double bench_scalar_mul() {
  return measure_ns([](int N) {
    Vec a(1.0, 2.0, 3.0);
    double f = 0.5;
    double acc = 0;
    for (int i = 0; i < N; ++i) {
      asm volatile("" : "+m"(a), "+m"(f));
      auto r = a * f;
      acc += r.x();
    }
    g_sink += acc;
  });
}

template<typename Vec>
double bench_cross() {
  return measure_ns([](int N) {
    Vec a(1.0, 2.0, 3.0), b(4.0, 5.0, 6.0);
    double acc = 0;
    for (int i = 0; i < N; ++i) {
      asm volatile("" : "+m"(a), "+m"(b));
      auto r = a.crossProduct(b);
      acc += r.x();
    }
    g_sink += acc;
  });
}

template<typename Vec>
double bench_normalize() {
  return measure_ns([](int N) {
    Vec a(1.0, 2.0, 3.0);
    double acc = 0;
    for (int i = 0; i < N; ++i) {
      asm volatile("" : "+m"(a));
      auto r = a.normalized();
      acc += r.x();
    }
    g_sink += acc;
  });
}

template<typename Vec>
double bench_reflect() {
  return measure_ns([](int N) {
    Vec iv(1.0, -1.0, 0.0), n(0.0, 1.0, 0.0);
    double acc = 0;
    for (int k = 0; k < N; ++k) {
      asm volatile("" : "+m"(iv), "+m"(n));
      double d = iv * n;
      auto r = iv - n * (2.0 * d);
      acc += r.x();
    }
    g_sink += acc;
  });
}

template<typename Vec>
void print_row(const char* name) {
  printf("  %-18s %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f\n", name, bench_dot<Vec>(), bench_add<Vec>(),
         bench_scalar_mul<Vec>(), bench_cross<Vec>(), bench_normalize<Vec>(), bench_reflect<Vec>());
}

int main() {
  printf("Phase 2.3 benchmark — Vector3<double> specialization options\n");
  printf("All times in ns/op. Lower is better.\n\n");
  printf("  %-18s %8s %8s %8s %8s %8s %8s\n", "Implementation", "dot", "add", "s*mul", "cross",
         "norm", "reflect");
  printf("  %-18s %8s %8s %8s %8s %8s %8s\n", "------------------", "--------", "--------",
         "--------", "--------", "--------", "--------");

  print_row<Vec3dSSE3>("baseline-sse3");
  print_row<Vec3dFixedSSE3>("option-C-fixed-sse3");
  print_row<Vec3dScalar>("option-A-scalar");
#ifdef __AVX2__
  print_row<Vec3dAVX2>("option-B-avx2");
#endif

  printf("\n");
  volatile double s = g_sink;
  (void)s;
  return 0;
}
