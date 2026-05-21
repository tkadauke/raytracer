#pragma once

#ifdef __SSE2__

#include <emmintrin.h>

// ---------------------------------------------------------------------------
// Matrix4<double> operator*(Matrix4<double>) — SSE2 matmul (double precision)
//
// Each double row is 4 × 8 = 32 bytes = two __m128d registers (lo=cols 0-1,
// hi=cols 2-3).  Strategy: load B's 8 half-rows once; for each row r of A
// broadcast each of 4 scalars via _mm_unpacklo/hi_pd and accumulate:
//   result_row_r_lo = A[r][0]*B0_lo + A[r][1]*B1_lo + A[r][2]*B2_lo + A[r][3]*B3_lo
//   result_row_r_hi = (same with _hi registers)
// ---------------------------------------------------------------------------
template<>
[[nodiscard]] inline Matrix4<double>
Matrix4<double>::operator*(const Matrix4<double>& b) const noexcept {
  const __m128d b0_lo = _mm_loadu_pd(b[0]);
  const __m128d b0_hi = _mm_loadu_pd(b[0] + 2);
  const __m128d b1_lo = _mm_loadu_pd(b[1]);
  const __m128d b1_hi = _mm_loadu_pd(b[1] + 2);
  const __m128d b2_lo = _mm_loadu_pd(b[2]);
  const __m128d b2_hi = _mm_loadu_pd(b[2] + 2);
  const __m128d b3_lo = _mm_loadu_pd(b[3]);
  const __m128d b3_hi = _mm_loadu_pd(b[3] + 2);

  Matrix4<double> result;
  for (int r = 0; r < 4; ++r) {
    const __m128d a_lo = _mm_loadu_pd((*this)[r]);
    const __m128d a_hi = _mm_loadu_pd((*this)[r] + 2);

    const __m128d a0 = _mm_unpacklo_pd(a_lo, a_lo);  // [A[r][0], A[r][0]]
    const __m128d a1 = _mm_unpackhi_pd(a_lo, a_lo);  // [A[r][1], A[r][1]]
    const __m128d a2 = _mm_unpacklo_pd(a_hi, a_hi);  // [A[r][2], A[r][2]]
    const __m128d a3 = _mm_unpackhi_pd(a_hi, a_hi);  // [A[r][3], A[r][3]]

    const __m128d res_lo = _mm_add_pd(
      _mm_add_pd(_mm_mul_pd(a0, b0_lo), _mm_mul_pd(a1, b1_lo)),
      _mm_add_pd(_mm_mul_pd(a2, b2_lo), _mm_mul_pd(a3, b3_lo))
    );
    const __m128d res_hi = _mm_add_pd(
      _mm_add_pd(_mm_mul_pd(a0, b0_hi), _mm_mul_pd(a1, b1_hi)),
      _mm_add_pd(_mm_mul_pd(a2, b2_hi), _mm_mul_pd(a3, b3_hi))
    );

    _mm_storeu_pd(&result[r][0], res_lo);
    _mm_storeu_pd(&result[r][2], res_hi);
  }
  return result;
}

#endif  // __SSE2__

#ifdef __SSE3__

#include <pmmintrin.h>

// ---------------------------------------------------------------------------
// Matrix4<double> operator*(Vector4<double>) — SSE3 mat-vec (double precision)
//
// Analogous to the float hadd strategy: multiply each row of M by v pair-wise,
// then reduce each pair of 2-element __m128d vectors using haddpd:
//
//   For each row r: p_lo = M_row_r[0:1] * v[0:1],  p_hi = M_row_r[2:3] * v[2:3]
//   t = haddpd(p_lo, p_hi) = [p_lo[0]+p_lo[1], p_hi[0]+p_hi[1]]
//                           = [M[r][0]*v[0]+M[r][1]*v[1], M[r][2]*v[2]+M[r][3]*v[3]]
//   result[r] = t[0] + t[1]  (scalar sum of the two __m128d lanes)
//
// Total: 2 loads (v_lo, v_hi) + per-row: 2 loads + 2 mulpd + 1 haddpd + scalar add.
// ---------------------------------------------------------------------------
template<>
[[nodiscard]] inline Vector4<double>
Matrix4<double>::operator*(const Vector4<double>& vec) const noexcept {
  const __m128d v_lo = _mm_loadu_pd(&vec[0]);  // [v[0], v[1]]
  const __m128d v_hi = _mm_loadu_pd(&vec[2]);  // [v[2], v[3]]

  double r[4];
  for (int row = 0; row < 4; ++row) {
    const __m128d t = _mm_hadd_pd(
      _mm_mul_pd(_mm_loadu_pd((*this)[row]),     v_lo),
      _mm_mul_pd(_mm_loadu_pd((*this)[row] + 2), v_hi)
    );
    r[row] = _mm_cvtsd_f64(t) + _mm_cvtsd_f64(_mm_unpackhi_pd(t, t));
  }
  return Vector4<double>(r[0], r[1], r[2], r[3]);
}

#endif  // __SSE3__
