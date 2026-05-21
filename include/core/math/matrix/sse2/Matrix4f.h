#pragma once

#ifdef __SSE__

#include <xmmintrin.h>

// ---------------------------------------------------------------------------
// Matrix4<float> operator*(Matrix4<float>) — SSE matmul
//
// Strategy: "column broadcast" — load all 4 rows of B once, then for each
// row r of A broadcast each of the 4 scalars via shufps and accumulate:
//   result_row_r = A[r][0]*B_row0 + A[r][1]*B_row1
//                + A[r][2]*B_row2 + A[r][3]*B_row3
//
// Total: 4 loads (B) + per-row: 1 load + 4 shufps + 4 mulps + 3 addps + 1 store.
// GCC/Clang with -O3 eliminates the identity-matrix initialization as dead stores.
// ---------------------------------------------------------------------------
template<>
[[nodiscard]] inline Matrix4<float>
Matrix4<float>::operator*(const Matrix4<float>& b) const noexcept {
  const __m128 b0 = _mm_loadu_ps(b[0]);
  const __m128 b1 = _mm_loadu_ps(b[1]);
  const __m128 b2 = _mm_loadu_ps(b[2]);
  const __m128 b3 = _mm_loadu_ps(b[3]);

  Matrix4<float> result;
  for (int r = 0; r < 4; ++r) {
    const __m128 a = _mm_loadu_ps((*this)[r]);
    const __m128 row = _mm_add_ps(
      _mm_add_ps(
        _mm_mul_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(0,0,0,0)), b0),
        _mm_mul_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(1,1,1,1)), b1)
      ),
      _mm_add_ps(
        _mm_mul_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(2,2,2,2)), b2),
        _mm_mul_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(3,3,3,3)), b3)
      )
    );
    _mm_storeu_ps(&result[r][0], row);
  }
  return result;
}

#endif  // __SSE__

#ifdef __SSE3__

#include <pmmintrin.h>

// ---------------------------------------------------------------------------
// Matrix4<float> operator*(Vector4<float>) — SSE3 mat-vec
//
// Strategy: element-wise multiply each row of M by v, then reduce each
// 4-element product vector to a scalar using two haddps passes:
//
//   p0 = row0 * v = [r0*v0, r1*v1, r2*v2, r3*v3]
//   p1 = row1 * v,  p2 = row2 * v,  p3 = row3 * v
//   t01 = haddps(p0, p1) = [r0*v0+r1*v1, r2*v2+r3*v3, s10+s11, s12+s13]
//   t23 = haddps(p2, p3)
//   out  = haddps(t01, t23) = [row0·v, row1·v, row2·v, row3·v]
//
// Total: 4 loads + 4 mulps + 3 haddps.  Requires SSE3 (-msse3).
// ---------------------------------------------------------------------------
template<>
[[nodiscard]] inline Vector4<float>
Matrix4<float>::operator*(const Vector4<float>& vec) const noexcept {
  const __m128 v = _mm_loadu_ps(&vec[0]);

  const __m128 t01 = _mm_hadd_ps(
    _mm_mul_ps(_mm_loadu_ps((*this)[0]), v),
    _mm_mul_ps(_mm_loadu_ps((*this)[1]), v)
  );
  const __m128 t23 = _mm_hadd_ps(
    _mm_mul_ps(_mm_loadu_ps((*this)[2]), v),
    _mm_mul_ps(_mm_loadu_ps((*this)[3]), v)
  );
  const __m128 result = _mm_hadd_ps(t01, t23);

  alignas(16) float r[4];
  _mm_store_ps(r, result);
  return Vector4<float>(r[0], r[1], r[2], r[3]);
}

#endif  // __SSE3__
