#pragma once

#ifndef RAYTRACER_CORE_MATH_MATRIX_H
#include "core/math/Matrix.h"
#endif

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
      for (int j = 0; j < col; ++j) {
        const VectorType qj = result.q.col(j);
        result.r[j][col] = qj * v;
        v -= qj * result.r[j][col];
      }

      T length = v.length();
      if (length <= tolerance) {
        v = zeroVector<Dimensions, T, VectorType, MatrixType>();
        v[col] = T(1);
        for (int j = 0; j < col; ++j) {
          const VectorType qj = result.q.col(j);
          v -= qj * (qj * v);
        }
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
  [[nodiscard]] inline MatrixType symmetricJacobiEigenvectors(MatrixType& a) noexcept {
    MatrixType v;
    const int maxIterations = 64;
    const T tolerance = std::numeric_limits<T>::epsilon() * T(64);

    for (int iteration = 0; iteration != maxIterations; ++iteration) {
      int p = 0;
      int q = 1;
      T maxOffDiagonal = T();
      for (int row = 0; row != Dimensions; ++row) {
        for (int col = row + 1; col != Dimensions; ++col) {
          const T magnitude = std::abs(a[row][col]);
          if (magnitude > maxOffDiagonal) {
            maxOffDiagonal = magnitude;
            p = row;
            q = col;
          }
        }
      }

      if (maxOffDiagonal <= tolerance) {
        break;
      }

      const T app = a[p][p];
      const T aqq = a[q][q];
      const T apq = a[p][q];
      const T tau = (aqq - app) / (T(2) * apq);
      const T t = std::copysign(T(1), tau) / (std::abs(tau) + std::sqrt(T(1) + tau * tau));
      const T c = T(1) / std::sqrt(T(1) + t * t);
      const T s = t * c;

      for (int k = 0; k != Dimensions; ++k) {
        if (k != p && k != q) {
          const T akp = a[k][p];
          const T akq = a[k][q];
          a[k][p] = a[p][k] = c * akp - s * akq;
          a[k][q] = a[q][k] = s * akp + c * akq;
        }
      }

      a[p][p] = c * c * app - T(2) * s * c * apq + s * s * aqq;
      a[q][q] = s * s * app + T(2) * s * c * apq + c * c * aqq;
      a[p][q] = a[q][p] = T();

      for (int k = 0; k != Dimensions; ++k) {
        const T vkp = v[k][p];
        const T vkq = v[k][q];
        v[k][p] = c * vkp - s * vkq;
        v[k][q] = s * vkp + c * vkq;
      }
    }

    return v;
  }

  template<int Dimensions, class T, class VectorType, class MatrixType>
  [[nodiscard]] inline SVD<Dimensions, T, VectorType, MatrixType>
  svd(const Matrix<Dimensions, T, VectorType, MatrixType>& matrix) {
    MatrixType ata = matrix.transposed() * MatrixType(matrix);
    MatrixType v = symmetricJacobiEigenvectors<Dimensions, T, VectorType, MatrixType>(ata);

    std::array<int, Dimensions> order;
    for (int i = 0; i != Dimensions; ++i) {
      order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](int left, int right) {
      return ata[left][left] > ata[right][right];
    });

    SVD<Dimensions, T, VectorType, MatrixType> result;
    result.u = zeroMatrix<Dimensions, T, VectorType, MatrixType>();
    result.v = zeroMatrix<Dimensions, T, VectorType, MatrixType>();

    const T tolerance = std::numeric_limits<T>::epsilon() *
                        std::max(T(1), matrix.norm1()) *
                        T(64);

    for (int outCol = 0; outCol != Dimensions; ++outCol) {
      const int sourceCol = order[outCol];
      const T eigenvalue = std::max(T(), ata[sourceCol][sourceCol]);
      result.singularValues[outCol] = std::sqrt(eigenvalue);
      result.v.setCol(outCol, v.col(sourceCol));

      VectorType uCol = matrix * result.v.col(outCol);
      if (result.singularValues[outCol] > tolerance) {
        uCol /= result.singularValues[outCol];
      }

      for (int previous = 0; previous < outCol; ++previous) {
        const VectorType q = result.u.col(previous);
        uCol -= q * (q * uCol);
      }

      if (uCol.length() <= tolerance) {
        uCol = zeroVector<Dimensions, T, VectorType, MatrixType>();
        uCol[outCol] = T(1);
        for (int previous = 0; previous < outCol; ++previous) {
          const VectorType q = result.u.col(previous);
          uCol -= q * (q * uCol);
        }
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
  const T determinantMagnitude = std::abs(determinant());

  // The determinant is the cheapest near-singularity screen available here:
  // ordinary transform matrices stay on the block-inverse fast path, while
  // matrices with tiny determinant pay for pivoted LU.
  constexpr T kFastPathThreshold = T(1e-5);
  if (std::isfinite(determinantMagnitude) && determinantMagnitude > kFastPathThreshold) {
    return inverted();
  }

  try {
    return this->luDecomposition().inverse();
  } catch (const DivisionByZeroException&) {
    return inverted();
  }
}
