#pragma once

#include "core/math/Matrix.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "core/DivisionByZeroException.h"

namespace matrix_decomposition {
  template<int Dimensions, class T, class VectorType, class MatrixType>
  [[nodiscard]] inline VectorType zeroVector() noexcept {
    VectorType result;
    for (int i = 0; i != Dimensions; ++i) {
      result[i] = T();
    }
    return result;
  }

  template<int Dimensions, class T, class VectorType, class MatrixType>
  [[nodiscard]] inline MatrixType zeroMatrix() noexcept {
    return MatrixType() * T();
  }

  template<int Dimensions, class T, class VectorType, class MatrixType>
  [[nodiscard]] inline MatrixType diagonalMatrix(const VectorType& diagonal) noexcept {
    MatrixType result = zeroMatrix<Dimensions, T, VectorType, MatrixType>();
    for (int i = 0; i != Dimensions; ++i) {
      result[i][i] = diagonal[i];
    }
    return result;
  }

  template<int Dimensions, class T, class VectorType, class MatrixType>
  [[nodiscard]] inline VectorType normalizedOrZero(const VectorType& vector, T tolerance) {
    const T length = vector.length();
    if (length <= tolerance) {
      return zeroVector<Dimensions, T, VectorType, MatrixType>();
    }
    return vector / length;
  }

  template<int Dimensions, class T, class VectorType, class MatrixType>
  [[nodiscard]] inline VectorType orthogonalizedBasisVector(
    const MatrixType& basis,
    int basisColumns,
    int preferredColumn,
    T tolerance
  ) {
    VectorType best = zeroVector<Dimensions, T, VectorType, MatrixType>();
    T bestSquaredLength = T(-1);

    for (int attempt = 0; attempt != Dimensions; ++attempt) {
      const int axis = (preferredColumn + attempt) % Dimensions;
      VectorType candidate = zeroVector<Dimensions, T, VectorType, MatrixType>();
      candidate[axis] = T(1);

      for (int previous = 0; previous < basisColumns; ++previous) {
        const VectorType q = basis.col(previous);
        candidate -= q * (q * candidate);
      }

      const T squaredLength = candidate.squaredLength();
      if (squaredLength > bestSquaredLength) {
        best = candidate;
        bestSquaredLength = squaredLength;
      }
    }

    if (bestSquaredLength <= tolerance * tolerance) {
      return zeroVector<Dimensions, T, VectorType, MatrixType>();
    }
    return best / std::sqrt(bestSquaredLength);
  }

  template<int Dimensions, class T, class VectorType, class MatrixType>
  struct LU {
    MatrixType lu;
    std::array<int, Dimensions> pivot;
    int swapCount = 0;
    bool singular = false;

    [[nodiscard]] MatrixType lower() const noexcept {
      MatrixType result = zeroMatrix<Dimensions, T, VectorType, MatrixType>();
      for (int row = 0; row != Dimensions; ++row) {
        result[row][row] = T(1);
        for (int col = 0; col < row; ++col) {
          result[row][col] = lu[row][col];
        }
      }
      return result;
    }

    [[nodiscard]] MatrixType upper() const noexcept {
      MatrixType result = zeroMatrix<Dimensions, T, VectorType, MatrixType>();
      for (int row = 0; row != Dimensions; ++row) {
        for (int col = row; col != Dimensions; ++col) {
          result[row][col] = lu[row][col];
        }
      }
      return result;
    }

    [[nodiscard]] MatrixType permutation() const noexcept {
      MatrixType result = zeroMatrix<Dimensions, T, VectorType, MatrixType>();
      for (int row = 0; row != Dimensions; ++row) {
        result[row][pivot[row]] = T(1);
      }
      return result;
    }

    [[nodiscard]] T determinant() const noexcept {
      if (singular) {
        return T();
      }

      T result = swapCount % 2 == 0 ? T(1) : T(-1);
      for (int i = 0; i != Dimensions; ++i) {
        result *= lu[i][i];
      }
      return result;
    }

    [[nodiscard]] VectorType solve(const VectorType& rhs) const {
      if (singular) {
        throw DivisionByZeroException(__FILE__, __LINE__);
      }

      VectorType x;
      for (int row = 0; row != Dimensions; ++row) {
        x[row] = rhs[pivot[row]];
      }

      for (int row = 0; row != Dimensions; ++row) {
        for (int col = 0; col < row; ++col) {
          x[row] -= lu[row][col] * x[col];
        }
      }

      for (int row = Dimensions - 1; row >= 0; --row) {
        for (int col = row + 1; col != Dimensions; ++col) {
          x[row] -= lu[row][col] * x[col];
        }
        if (lu[row][row] == T()) {
          throw DivisionByZeroException(__FILE__, __LINE__);
        }
        x[row] /= lu[row][row];
      }
      return x;
    }

    [[nodiscard]] MatrixType inverse() const {
      MatrixType result = zeroMatrix<Dimensions, T, VectorType, MatrixType>();
      for (int col = 0; col != Dimensions; ++col) {
        VectorType basis = zeroVector<Dimensions, T, VectorType, MatrixType>();
        basis[col] = T(1);
        result.setCol(col, solve(basis));
      }
      return result;
    }
  };

  template<int Dimensions, class T, class VectorType, class MatrixType>
  [[nodiscard]] inline LU<Dimensions, T, VectorType, MatrixType>
  lu(const Matrix<Dimensions, T, VectorType, MatrixType>& matrix) noexcept {
    LU<Dimensions, T, VectorType, MatrixType> result;
    result.lu = MatrixType(matrix);
    for (int i = 0; i != Dimensions; ++i) {
      result.pivot[i] = i;
    }

    const T tolerance = std::numeric_limits<T>::epsilon() *
                        std::max(T(1), matrix.norm1()) *
                        T(Dimensions);

    for (int col = 0; col != Dimensions; ++col) {
      int pivotRow = col;
      T pivotMagnitude = std::abs(result.lu[col][col]);
      for (int row = col + 1; row != Dimensions; ++row) {
        const T magnitude = std::abs(result.lu[row][col]);
        if (magnitude > pivotMagnitude) {
          pivotMagnitude = magnitude;
          pivotRow = row;
        }
      }

      if (pivotMagnitude <= tolerance) {
        result.singular = true;
        return result;
      }

      if (pivotRow != col) {
        std::swap(result.lu[pivotRow], result.lu[col]);
        std::swap(result.pivot[pivotRow], result.pivot[col]);
        ++result.swapCount;
      }

      for (int row = col + 1; row != Dimensions; ++row) {
        result.lu[row][col] /= result.lu[col][col];
        for (int k = col + 1; k != Dimensions; ++k) {
          result.lu[row][k] -= result.lu[row][col] * result.lu[col][k];
        }
      }
    }

    return result;
  }

  template<int Dimensions, class T, class VectorType, class MatrixType>
  struct QR {
    MatrixType q;
    MatrixType r;
  };

  template<int Dimensions, class T, class VectorType, class MatrixType>
  [[nodiscard]] inline QR<Dimensions, T, VectorType, MatrixType>
  qr(const Matrix<Dimensions, T, VectorType, MatrixType>& matrix) {
    QR<Dimensions, T, VectorType, MatrixType> result;
    result.q = zeroMatrix<Dimensions, T, VectorType, MatrixType>();
    result.r = zeroMatrix<Dimensions, T, VectorType, MatrixType>();

    const T tolerance = std::numeric_limits<T>::epsilon() *
                        std::max(T(1), matrix.norm1()) *
                        T(64);

    for (int col = 0; col != Dimensions; ++col) {
      VectorType v = matrix.col(col);
      for (int pass = 0; pass != 2; ++pass) {
        for (int j = 0; j < col; ++j) {
          const VectorType qj = result.q.col(j);
          const T projection = qj * v;
          result.r[j][col] += projection;
          v -= qj * projection;
        }
      }

      T length = v.length();
      if (length <= tolerance) {
        v = orthogonalizedBasisVector<Dimensions, T, VectorType, MatrixType>(
          result.q,
          col,
          col,
          tolerance
        );
        length = v.length();
      }

      if (length <= tolerance) {
        result.r[col][col] = T();
        continue;
      }

      const VectorType q = v / length;
      result.r[col][col] = length;
      result.q.setCol(col, q);
    }

    return result;
  }

  template<int Dimensions, class T, class VectorType, class MatrixType>
  struct SVD {
    MatrixType u;
    VectorType singularValues;
    MatrixType v;

    [[nodiscard]] MatrixType sigma() const noexcept {
      return diagonalMatrix<Dimensions, T, VectorType, MatrixType>(singularValues);
    }
  };

  template<int Dimensions, class T, class VectorType, class MatrixType>
  [[nodiscard]] inline SVD<Dimensions, T, VectorType, MatrixType>
  svd(const Matrix<Dimensions, T, VectorType, MatrixType>& matrix) {
    MatrixType columns(matrix);
    MatrixType v;
    const T tolerance = std::numeric_limits<T>::epsilon() *
                        std::max(T(1), matrix.norm1()) *
                        T(64);

    for (int sweep = 0; sweep != 32; ++sweep) {
      bool changed = false;
      for (int p = 0; p != Dimensions - 1; ++p) {
        for (int q = p + 1; q != Dimensions; ++q) {
          const VectorType colP = columns.col(p);
          const VectorType colQ = columns.col(q);
          const T alpha = colP * colP;
          const T beta = colQ * colQ;
          const T gamma = colP * colQ;
          const T scale = std::sqrt(std::max(T(), alpha * beta));
          if (scale <= tolerance || std::abs(gamma) <= tolerance * scale) {
            continue;
          }

          const T tau = (beta - alpha) / (T(2) * gamma);
          const T sign = tau >= T() ? T(1) : T(-1);
          const T t = sign / (std::abs(tau) + std::sqrt(T(1) + tau * tau));
          const T c = T(1) / std::sqrt(T(1) + t * t);
          const T s = t * c;

          for (int row = 0; row != Dimensions; ++row) {
            const T oldP = columns[row][p];
            const T oldQ = columns[row][q];
            columns[row][p] = c * oldP - s * oldQ;
            columns[row][q] = s * oldP + c * oldQ;

            const T oldVP = v[row][p];
            const T oldVQ = v[row][q];
            v[row][p] = c * oldVP - s * oldVQ;
            v[row][q] = s * oldVP + c * oldVQ;
          }
          changed = true;
        }
      }

      if (!changed) {
        break;
      }
    }

    std::array<int, Dimensions> order;
    for (int i = 0; i != Dimensions; ++i) {
      order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](int left, int right) {
      return columns.col(left).squaredLength() > columns.col(right).squaredLength();
    });

    SVD<Dimensions, T, VectorType, MatrixType> result;
    result.u = zeroMatrix<Dimensions, T, VectorType, MatrixType>();
    result.v = zeroMatrix<Dimensions, T, VectorType, MatrixType>();

    for (int outCol = 0; outCol != Dimensions; ++outCol) {
      const int sourceCol = order[outCol];
      result.singularValues[outCol] = columns.col(sourceCol).length();
      result.v.setCol(outCol, v.col(sourceCol));

      VectorType uCol = columns.col(sourceCol);
      if (result.singularValues[outCol] > tolerance) {
        uCol /= result.singularValues[outCol];
      }

      for (int previous = 0; previous < outCol; ++previous) {
        const VectorType q = result.u.col(previous);
        uCol -= q * (q * uCol);
      }

      if (uCol.length() <= tolerance) {
        uCol = orthogonalizedBasisVector<Dimensions, T, VectorType, MatrixType>(
          result.u,
          outCol,
          outCol,
          tolerance
        );
      }

      result.u.setCol(outCol, normalizedOrZero<Dimensions, T, VectorType, MatrixType>(uCol, tolerance));
    }

    return result;
  }
}

template<int Dimensions, class T, class VectorType, class Derived>
matrix_decomposition::LU<
  Dimensions,
  T,
  VectorType,
  typename Matrix<Dimensions, T, VectorType, Derived>::MatrixType>
Matrix<Dimensions, T, VectorType, Derived>::luDecomposition() const noexcept {
  return matrix_decomposition::lu<
    Dimensions,
    T,
    VectorType,
    typename Matrix<Dimensions, T, VectorType, Derived>::MatrixType>(*this);
}

template<int Dimensions, class T, class VectorType, class Derived>
matrix_decomposition::QR<
  Dimensions,
  T,
  VectorType,
  typename Matrix<Dimensions, T, VectorType, Derived>::MatrixType>
Matrix<Dimensions, T, VectorType, Derived>::qrDecomposition() const {
  return matrix_decomposition::qr<
    Dimensions,
    T,
    VectorType,
    typename Matrix<Dimensions, T, VectorType, Derived>::MatrixType>(*this);
}

template<int Dimensions, class T, class VectorType, class Derived>
matrix_decomposition::SVD<
  Dimensions,
  T,
  VectorType,
  typename Matrix<Dimensions, T, VectorType, Derived>::MatrixType>
Matrix<Dimensions, T, VectorType, Derived>::svdDecomposition() const {
  return matrix_decomposition::svd<
    Dimensions,
    T,
    VectorType,
    typename Matrix<Dimensions, T, VectorType, Derived>::MatrixType>(*this);
}

template<int Dimensions, class T, class VectorType, class Derived>
[[nodiscard]] inline matrix_decomposition::LU<
  Dimensions,
  T,
  VectorType,
  typename Matrix<Dimensions, T, VectorType, Derived>::MatrixType>
luDecomposition(const Matrix<Dimensions, T, VectorType, Derived>& matrix) noexcept {
  return matrix.luDecomposition();
}

template<int Dimensions, class T, class VectorType, class Derived>
[[nodiscard]] inline matrix_decomposition::QR<
  Dimensions,
  T,
  VectorType,
  typename Matrix<Dimensions, T, VectorType, Derived>::MatrixType>
qrDecomposition(const Matrix<Dimensions, T, VectorType, Derived>& matrix) {
  return matrix.qrDecomposition();
}

template<int Dimensions, class T, class VectorType, class Derived>
[[nodiscard]] inline matrix_decomposition::SVD<
  Dimensions,
  T,
  VectorType,
  typename Matrix<Dimensions, T, VectorType, Derived>::MatrixType>
svdDecomposition(const Matrix<Dimensions, T, VectorType, Derived>& matrix) {
  return matrix.svdDecomposition();
}

template<class T>
Matrix4<T> Matrix4<T>::stableInverse() const {
  try {
    const Matrix4<T> blockInverse = inverted();
    const T conditionEstimate = this->norm1() * blockInverse.norm1();

    // Estimate kappa_1(A) = ||A||_1 * ||A^-1||_1 from the already-computed
    // fast inverse. Well-conditioned transforms return the block inverse;
    // ill-conditioned matrices pay for pivoted LU to avoid Schur-complement
    // cancellation near singularity.
    constexpr T kConditionThreshold = T(100000);
    if (std::isfinite(conditionEstimate) && conditionEstimate <= kConditionThreshold) {
      return blockInverse;
    }
  } catch (const DivisionByZeroException&) {
    // The block inverse can fail on singular Schur-complement pivots even when
    // the full matrix is invertible. Pivoted LU gets the final decision.
  }

  try {
    return this->luDecomposition().inverse();
  } catch (const DivisionByZeroException&) {
    return inverted();
  }
}
