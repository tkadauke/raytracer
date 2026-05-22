#pragma once

#include <functional>
#include <iostream>
#include <cmath>
#include <type_traits>
#include <algorithm>
#include <array>
#include <limits>
#include "core/math/Vector.h"
#include "core/math/Angle.h"
#include "core/DivisionByZeroException.h"

template<class T>
class Quaternion;

namespace matrix_decomposition {
  template<int Dimensions, class T, class VectorType, class MatrixType>
  struct LU;

  template<int Dimensions, class T, class VectorType, class MatrixType>
  struct QR;

  template<int Dimensions, class T, class VectorType, class MatrixType>
  struct SVD;
}

/**
  * Represents a square matrix with arbitrary, but fixed size. It implements
  * many matrix operations in a generic way.
  * 
  * This class is designed to be inherited from. There are specialized versions
  * for 2x2, 3x3 and 4x4 matrixes: Matrix2, Matrix3, and Matrix4.
  * 
  * @tparam Dimensions the size of the Matrix type.
  * @tparam T the Matrix's number type.
  * @tparam VectorType the Vector type used for Matrix-Vector calculations.
  * @tparam Derived the derived class, if any. Defaults to void. If
  *   this is void, then all the calculation operations, like +,
  *   accept as argument and return an object of type Matrix. If Derived is set
  *   explicitely, then the operators accept and return objects of type Derived.
  *   This way, operators don't have to be redefined in the subclasses. See
  *   Matrix::MatrixType for details.
  */
template<int Dimensions, class T, class VectorType = Vector<Dimensions, T>, class Derived = void>
class Matrix {
  typedef T RowType[Dimensions];
  typedef RowType CellsType[Dimensions];
  
  typedef Matrix<Dimensions, T, VectorType, Derived> ThisType;

public:
  /**
    * Type for arguments and return types of many operators defined in this
    * class. If the Derived template parameter is omitted, then this is
    * equivalent to Matrix. Otherwise, it is equivalent to Derived.
    */
  using MatrixType = std::conditional_t<
    std::is_same_v<Derived, void>,
    ThisType,
    Derived
  >;
  
  /**
    * Vector type for Matrix-Vector calculations.
    */
  typedef VectorType Vector;
  
  /**
    * Constructs the identity matrix of size Dim:
    * 
    * \f[\left(\begin{array}{cccc}
    *   1      & 0      & \ldots & 0      \\
    *   0      & 1      & 0      & \vdots \\
    *   \vdots & 0      & \ddots & 0      \\
    *   0      & \ldots & 0      & 1
    * \end{array}\right)\f]
    */
  inline constexpr Matrix() {
    for (int row = 0; row != Dimensions; ++row) {
      for (int col = 0; col != Dimensions; ++col) {
        if (row == col) {
          m_cells[row][col] = 1;
        } else {
          m_cells[row][col] = T();
        }
      }
    }
  }

  /**
    * Constructs a matrix from cells, a two-dimensional array of size Dim.
    */
  inline constexpr explicit Matrix(const CellsType& cells) {
    for (int row = 0; row != Dimensions; ++row) {
      for (int col = 0; col != Dimensions; ++col) {
        m_cells[row][col] = cells[row][col];
      }
    }
  }

  /**
    * Constructs a matrix from source, which can be of a different Matrix type.
    * The source Matrix can be of different size. If bigger, only what fits into
    * the destination matrix will be copied. If smaller, the rest will be filled
    * with zeroes, except the diagonal, which will be filled with ones.
    */
  template<int D, class V, class M>
  inline constexpr Matrix(const Matrix<D, T, V, M>& source) {
    for (int row = 0; row != Dimensions; ++row) {
      for (int col = 0; col != Dimensions; ++col) {
        if (row >= D || col >= D) {
          if (row == col)
            m_cells[row][col] = T(1);
          else
            m_cells[row][col] = T();
        } else {
          m_cells[row][col] = source[row][col];
        }
      }
    }
  }

  /**
    * @returns the cell at coordinate (row, col).
    */
  [[nodiscard]] inline constexpr const T& cell(int row, int col) const noexcept {
    return m_cells[row][col];
  }

  /**
    * Sets the cell at coordinate (row, col) to value.
    */
  inline constexpr void setCell(int row, int col, const T& value) noexcept {
    m_cells[row][col] = value;
  }

  /**
    * @returns the row at index as a constant array.
    */
  [[nodiscard]] inline constexpr const RowType& operator[](int index) const noexcept {
    return m_cells[index];
  }

  /**
    * @returns the row at index as a mutable array.
    */
  inline constexpr RowType& operator[](int index) noexcept {
    return m_cells[index];
  }

  /**
    * @returns true, if this matrix equals other, false otherwise.
    */
  [[nodiscard]] inline constexpr bool operator==(const MatrixType& other) const noexcept {
    for (int row = 0; row != Dimensions; ++row) {
      for (int col = 0; col != Dimensions; ++col) {
        if (m_cells[row][col] != other[row][col])
          return false;
      }
    }
    return true;
  }

  /**
    * @returns true if this matrix is different from other, false otherwise.
    */
  [[nodiscard]] inline constexpr bool operator!=(const MatrixType& other) const noexcept {
    return !(*this == other);
  }

  /**
    * @returns the matrix multiplication of this matrix and other.
    */
  [[nodiscard]] inline constexpr MatrixType operator*(const MatrixType& other) const noexcept {
    MatrixType result;

    for (int row = 0; row != Dimensions; ++row) {
      for (int col = 0; col != Dimensions; ++col) {
        T cell = T();
        for (int i = 0; i != Dimensions; ++i) {
          cell += m_cells[row][i] * other[i][col];
        }
        result[row][col] = cell;
      }
    }
    return result;
  }

  /**
    * Multiplies this matrix with other in-place.
    */
  inline constexpr MatrixType& operator*=(const MatrixType& other) noexcept {
    *this = *this * other;
    return static_cast<MatrixType&>(*this);
  }

  /**
    * @returns the Vector that is the result of the multiplication of this
    *   Matrix and vector.
    */
  [[nodiscard]] inline constexpr Vector operator*(const Vector& vector) const noexcept {
    Vector result;

    for (int row = 0; row != Dimensions; ++row) {
      T cell = T();
      for (int i = 0; i != Dimensions; ++i) {
        cell += m_cells[row][i] * vector[i];
      }
      result[row] = cell;
    }
    return result;
  }

  /**
    * @returns the Matrix that is the result of the multiplication of this
    *   Matrix \f$M\f$ with scalar \f$c\f$.
    *
    *   \f[
    *   Mc =
    *   \left(\begin{array}{cccc}
    *     m_{00}c & m_{01}c & \ldots & m_{0n}c \\
    *     m_{10}c & m_{11}c & \ldots & m_{1n}c \\
    *     \vdots  & \vdots  & \ddots & \vdots \\
    *     m_{n0}c & m_{n1}c & \ldots & m_{nn}c
    *   \end{array}\right)\f]
    */
  [[nodiscard]] inline constexpr MatrixType operator*(const T& scalar) const noexcept {
    MatrixType result;

    for (int row = 0; row != Dimensions; ++row) {
      for (int col = 0; col != Dimensions; ++col) {
        result[row][col] = m_cells[row][col] * scalar;
      }
    }
    return result;
  }

  /**
    * Multiplies this matrix with scalar in-place.
    */
  inline constexpr MatrixType& operator*=(const T& scalar) noexcept {
    *this = *this * scalar;
    return static_cast<MatrixType&>(*this);
  }

  /**
    * @returns the Matrix that is the result of the addition of this matrix
    *   \f$A\f$ and other \f$B\f$.
    *
    *   \f[
    *   A + B =
    *   \left(\begin{array}{cccc}
    *     a_{00} + b_{00} & a_{01} + b_{01} & \ldots & a_{0n} + b_{0n} \\
    *     a_{10} + b_{10} & a_{11} + b_{11} & \ldots & a_{1n} + b_{1n} \\
    *     \vdots          & \vdots          & \ddots & \vdots \\
    *     a_{n0} + b_{n0} & a_{n1} + b_{n1} & \ldots & a_{nn} + b_{nn}
    *   \end{array}\right)\f]
    */
  [[nodiscard]] inline constexpr MatrixType operator+(const MatrixType& other) const noexcept {
    MatrixType result;

    for (int row = 0; row != Dimensions; ++row) {
      for (int col = 0; col != Dimensions; ++col) {
        result[row][col] = m_cells[row][col] + other.cell(row, col);
      }
    }
    return result;
  }

  /**
    * @returns the Matrix that is the result of this Matrix \f$M\f$ divided by
    *   scalar \f$c\f$.
    *
    *   \f[
    *   \frac{M}{c} =
    *   \left(\begin{array}{cccc}
    *     m_{00} / c & m_{01} / c & \ldots & m_{0n} / c \\
    *     m_{10} / c & m_{11} / c & \ldots & m_{1n} / c \\
    *     \vdots     & \vdots     & \ddots & \vdots \\
    *     m_{n0} / c & m_{n1} / c & \ldots & m_{nn} / c
    *   \end{array}\right)\f]
    */
  [[nodiscard]] inline MatrixType operator/(const T& scalar) const {
    if (scalar == T())
      throw DivisionByZeroException(__FILE__, __LINE__);

    MatrixType result;

    for (int row = 0; row != Dimensions; ++row) {
      for (int col = 0; col != Dimensions; ++col) {
        result[row][col] = m_cells[row][col] / scalar;
      }
    }
    return result;
  }

  /**
    * @returns the row vector for row.
    */
  [[nodiscard]] inline constexpr Vector row(int row) const noexcept {
    Vector result;
    for (int col = 0; col != Dimensions; ++col) {
      result[col] = m_cells[row][col];
    }
    return result;
  }

  /**
    * @returns the column vector for col.
    */
  [[nodiscard]] inline constexpr Vector col(int col) const noexcept {
    Vector result;
    for (int row = 0; row != Dimensions; ++row) {
      result[row] = m_cells[row][col];
    }
    return result;
  }

  /**
    * Sets the column at index col from vector.
    */
  inline constexpr void setCol(int col, const Vector& vector) noexcept {
    for (int row = 0; row != Dimensions; ++row) {
      m_cells[row][col] = vector[row];
    }
  }

  /**
    * @returns the sum of all elements in row.
    */
  [[nodiscard]] inline constexpr T rowSum(int row) const noexcept {
    T result = T();
    for (int col = 0; col != Dimensions; ++col) {
      result += m_cells[row][col];
    }
    return result;
  }

  /**
    * @returns the sum of all elements in col.
    */
  [[nodiscard]] inline constexpr T colSum(int col) const noexcept {
    T result = T();
    for (int row = 0; row != Dimensions; ++row) {
      result += m_cells[row][col];
    }
    return result;
  }

  /**
    * @returns the matrix 1-norm: the maximum absolute column sum.
    */
  [[nodiscard]] inline T norm1() const noexcept {
    T result = T();
    for (int col = 0; col != Dimensions; ++col) {
      T sum = T();
      for (int row = 0; row != Dimensions; ++row) {
        sum += std::abs(m_cells[row][col]);
      }
      result = std::max(result, sum);
    }
    return result;
  }

  /**
    * @returns the transpose of this Matrix.
    */
  [[nodiscard]] inline MatrixType transposed() const noexcept {
    MatrixType result(*this);

    for (int row = 0; row != Dimensions; ++row) {
      for (int col = row + 1; col != Dimensions; ++col) {
        std::swap(result[row][col], result[col][row]);
      }
    }
    return result;
  }

  /**
    * @returns the all-zero matrix.
    */
  [[nodiscard]] inline static constexpr MatrixType zero() noexcept {
    MatrixType result;
    for (int row = 0; row != Dimensions; ++row) {
      for (int col = 0; col != Dimensions; ++col) {
        result[row][col] = T();
      }
    }
    return result;
  }

  /**
    * @returns a zero matrix with diagonal on the main diagonal.
    */
  [[nodiscard]] inline static constexpr MatrixType diagonal(const Vector& diagonal) noexcept {
    MatrixType result = zero();
    for (int i = 0; i != Dimensions; ++i) {
      result[i][i] = diagonal[i];
    }
    return result;
  }

  /**
    * @returns the best coordinate-axis basis vector orthogonalized against
    * the first basisColumns columns of this matrix.
    */
  [[nodiscard]] inline Vector orthogonalizedBasisVector(
    int basisColumns,
    int preferredColumn,
    T tolerance
  ) const {
    Vector best = Vector::zero();
    T bestSquaredLength = T(-1);

    for (int attempt = 0; attempt != Dimensions; ++attempt) {
      const int axis = (preferredColumn + attempt) % Dimensions;
      Vector candidate = Vector::zero();
      candidate[axis] = T(1);

      for (int previous = 0; previous < basisColumns; ++previous) {
        const Vector q = col(previous);
        candidate -= q * (q * candidate);
      }

      const T squaredLength = candidate.squaredLength();
      if (squaredLength > bestSquaredLength) {
        best = candidate;
        bestSquaredLength = squaredLength;
      }
    }

    if (bestSquaredLength <= tolerance * tolerance) {
      return Vector::zero();
    }
    return best / std::sqrt(bestSquaredLength);
  }

  /**
    * @returns the LU decomposition of this matrix, with partial pivoting.
    */
  [[nodiscard]] inline matrix_decomposition::LU<Dimensions, T, Vector, MatrixType>
  luDecomposition() const noexcept;

  /**
    * @returns the QR decomposition of this matrix.
    */
  [[nodiscard]] inline matrix_decomposition::QR<Dimensions, T, Vector, MatrixType>
  qrDecomposition() const;

  /**
    * @returns the singular value decomposition of this matrix.
    */
  [[nodiscard]] inline matrix_decomposition::SVD<Dimensions, T, Vector, MatrixType>
  svdDecomposition() const;

private:
  CellsType m_cells;
};

namespace matrix_decomposition {
  template<int Dimensions, class T, class VectorType, class MatrixType>
  struct LU {
    MatrixType lu;
    std::array<int, Dimensions> pivot;
    int swapCount = 0;
    bool singular = false;

    [[nodiscard]] MatrixType lower() const noexcept {
      MatrixType result = MatrixType::zero();
      for (int row = 0; row != Dimensions; ++row) {
        result[row][row] = T(1);
        for (int col = 0; col < row; ++col) {
          result[row][col] = lu[row][col];
        }
      }
      return result;
    }

    [[nodiscard]] MatrixType upper() const noexcept {
      MatrixType result = MatrixType::zero();
      for (int row = 0; row != Dimensions; ++row) {
        for (int col = row; col != Dimensions; ++col) {
          result[row][col] = lu[row][col];
        }
      }
      return result;
    }

    [[nodiscard]] MatrixType permutation() const noexcept {
      MatrixType result = MatrixType::zero();
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

      VectorType x = VectorType::zero();
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
      MatrixType result = MatrixType::zero();
      for (int col = 0; col != Dimensions; ++col) {
        VectorType basis = VectorType::zero();
        basis[col] = T(1);
        result.setCol(col, solve(basis));
      }
      return result;
    }
  };

  template<int Dimensions, class T, class VectorType, class MatrixType>
  struct QR {
    MatrixType q;
    MatrixType r;
  };

  template<int Dimensions, class T, class VectorType, class MatrixType>
  struct SVD {
    MatrixType u;
    VectorType singularValues;
    MatrixType v;

    [[nodiscard]] MatrixType sigma() const noexcept {
      return MatrixType::diagonal(singularValues);
    }
  };
}

template<int Dimensions, class T, class VectorType, class Derived>
matrix_decomposition::LU<
  Dimensions,
  T,
  VectorType,
  typename Matrix<Dimensions, T, VectorType, Derived>::MatrixType>
Matrix<Dimensions, T, VectorType, Derived>::luDecomposition() const noexcept {
  using Result = matrix_decomposition::LU<Dimensions, T, VectorType, MatrixType>;

  Result result;
  result.lu = MatrixType(static_cast<const MatrixType&>(*this));
  for (int i = 0; i != Dimensions; ++i) {
    result.pivot[i] = i;
  }

  const T tolerance = std::numeric_limits<T>::epsilon() *
                      std::max(T(1), norm1()) *
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

template<int Dimensions, class T, class VectorType, class Derived>
matrix_decomposition::QR<
  Dimensions,
  T,
  VectorType,
  typename Matrix<Dimensions, T, VectorType, Derived>::MatrixType>
Matrix<Dimensions, T, VectorType, Derived>::qrDecomposition() const {
  using Result = matrix_decomposition::QR<Dimensions, T, VectorType, MatrixType>;

  Result result;
  result.q = MatrixType::zero();
  result.r = MatrixType::zero();

  const T tolerance = std::numeric_limits<T>::epsilon() *
                      std::max(T(1), norm1()) *
                      T(64);

  for (int col = 0; col != Dimensions; ++col) {
    VectorType v = this->col(col);
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
      v = result.q.orthogonalizedBasisVector(
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

template<int Dimensions, class T, class VectorType, class Derived>
matrix_decomposition::SVD<
  Dimensions,
  T,
  VectorType,
  typename Matrix<Dimensions, T, VectorType, Derived>::MatrixType>
Matrix<Dimensions, T, VectorType, Derived>::svdDecomposition() const {
  using Result = matrix_decomposition::SVD<Dimensions, T, VectorType, MatrixType>;

  MatrixType columns(static_cast<const MatrixType&>(*this));
  MatrixType v;
  const T tolerance = std::numeric_limits<T>::epsilon() *
                      std::max(T(1), norm1()) *
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

  Result result;
  result.u = MatrixType::zero();
  result.v = MatrixType::zero();
  result.singularValues = VectorType::zero();

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
      uCol = result.u.orthogonalizedBasisVector(
        outCol,
        outCol,
        tolerance
      );
    }

    result.u.setCol(outCol, uCol.normalizedOrZero(tolerance));
  }

  return result;
}

/**
  * Generic string serialization function for Matrixes. Turns a matrix
  * 
  * \f[\left(\begin{array}{cccc}
  *   1 & 0 \\
  *   0 & 1 \\
  * \end{array}\right)\f]
  *
  * into string \c "1 0\n0 1\n".
  *
  * @returns os.
  */
template<int Dimensions, class T, class VectorType, class Derived>
std::ostream& operator<<(std::ostream& os, const Matrix<Dimensions, T, VectorType, Derived>& matrix) {
  for (int row = 0; row != Dimensions; ++row) {
    for (int col = 0; col != Dimensions; ++col) {
      os << matrix[row][col] << ' ';
    }
    os << std::endl;
  }
  return os;
}

/**
  * Represents a two-dimensional matrix with component type T. This class
  * implements many important operations for matrixes. Many of those are
  * defined in the parent class. The operations defined in this class are
  * mostly specific to two-dimensional matrixes.
  * 
  * Use this class to transform absolute points on the plane, directional
  * vectors, or normals.
  * 
  * This class defines a number of different constant matrixes, such as
  * identity(), rotate90(), or reflectX().
  * 
  * Construction of matrixes is as expected. The default constructor creates the
  * identity matrix, there is a component-wise constructor, and a generic copy
  * constructor that converts any matrix type to a two-dimensional matrix.
  * 
  * Additionally, there are a lot of special static construction methods, like
  * rotate(), scale() or shear().
  *
  * @tparam T the coordinate type, usually a floating point type.
  */
template<class T>
class Matrix2 : public Matrix<2, T, Vector2<T>, Matrix2<T>> {
  typedef Matrix<2, T, Vector2<T>, Matrix2<T>> Base;
public:
  using Base::cell;
  using Base::setCell;
  
  /**
    * Default constructor. Constructs the identity matrix
    * 
    * \f[\left(\begin{array}{cccc}
    *   1 & 0 \\
    *   0 & 1 \\
    * \end{array}\right)\f]
    */
  inline constexpr Matrix2()
    : Base()
  {
  }

  /**
    * Constructs a matrix from source, which can be of a different Matrix type.
    * The source Matrix can be of different size, but only the top left 2x2
    * submatrix is copied. If the source is smaller, then the rest is filled
    * with zeroes, except for the diagonal.
    */
  template<int D, class V, class M>
  inline constexpr Matrix2(const Matrix<D, T, V, M>& source)
    : Base(source)
  {
  }

  /**
    * Component-wise constructor for a Matrix2.
    */
  inline constexpr Matrix2(const T& c00, const T& c01,
                            const T& c10, const T& c11) {
    setCell(0, 0, c00); setCell(0, 1, c01);
    setCell(1, 0, c10); setCell(1, 1, c11);
  }

  /**
    * @returns this matrix' determinant.
    */
  [[nodiscard]] inline constexpr T determinant() const noexcept {
    return cell(0, 0) * cell(1, 1) - cell(0, 1) * cell(1, 0);
  }

  /**
    * @returns the inverse \f$M'\f$ of this matrix \f$M\f$, so that \f$M \times
    *   M' = I\f$, where \f$I\f$ is the identity matrix.
    */
  [[nodiscard]] inline Matrix2<T> inverted() const {
    return Matrix2<T>(cell(1, 1), -cell(0, 1), -cell(1, 0), cell(0, 0)) / determinant();
  }

  /**
    * @returns a matrix that represents a rotation around the origin with angle.
    */
  template<class A>
  [[nodiscard]] inline static Matrix2<T> rotate(const A& angle) noexcept {
    T sin = std::sin(angle.radians()), cos = std::cos(angle.radians());
    return Matrix2<T>(cos, -sin,
                      sin, cos);
  }

  /**
    * @returns a matrix that represents a clockwise rotation around the origin
    *   with angle.
    */
  template<class A>
  [[nodiscard]] inline static Matrix2<T> clockwise(const A& angle) noexcept {
    return rotate(-angle);
  }

  /**
    * @returns a matrix that represents a counter-clockwise rotation around the
    *   origin with angle.
    */
  template<class A>
  [[nodiscard]] inline static Matrix2<T> counterclockwise(const A& angle) noexcept {
    return rotate(angle);
  }

  /**
    * @returns a matrix that represents a scaling of factor.
    */
  [[nodiscard]] inline static constexpr Matrix2<T> scale(const T& factor) noexcept {
    return Matrix2<T>(factor, 0,
                      0,      factor);
  }

  /**
    * @returns a matrix that represents a horizontal scaling of xFactor, and
    *   vertical scaling of yFactor.
    */
  [[nodiscard]] inline static constexpr Matrix2<T> scale(const T& xFactor, const T& yFactor) noexcept {
    return Matrix2<T>(xFactor, 0,
                      0,       yFactor);
  }

  /**
    * @returns a matrix that represents a horizontal sharing of xShear, and
    *   vertical sharing of yShear.
    */
  [[nodiscard]] inline static constexpr Matrix2<T> shear(const T& xShear, const T& yShear) noexcept {
    return Matrix2<T>(1,      xShear,
                      yShear, 1);
  }

  /**
    * @returns a matrix that represents a reflection along vector.
    */
  [[nodiscard]] inline static Matrix2<T> reflect(const Vector2<T>& vector) noexcept {
    return reflect(vector.x(), vector.y());
  }

  /**
    * @returns a matrix that represents a reflection along the line going
    *   through the origin and \f$(x, y)\f$.
    */
  [[nodiscard]] inline static Matrix2<T> reflect(const T& x, const T& y) noexcept {
    T len = std::sqrt(x * x + y * y);
    T divider = len*len;

    T coordProduct = (2 * x * y) / divider;

    return Matrix2<T>((x*x - y*y) / divider, coordProduct,
                      coordProduct,          (y*y - x*x) / divider);
  }

  /**
    * @returns a matrix that represents a reflection along vector.
    */
  [[nodiscard]] inline static Matrix2<T> project(const Vector2<T>& vector) noexcept {
    return project(vector.x(), vector.y());
  }

  /**
    * @returns an orthogonal projection matrix, that projects vectors onto the
    *   line going through the origin and \f$(x, y)\f$.
    */
  [[nodiscard]] inline static Matrix2<T> project(const T& x, const T& y) noexcept {
    T len = std::sqrt(x * x + y * y);
    T divider = len*len;

    T coordProduct = (x * y) / divider;

    return Matrix2<T>((x * x) / divider, coordProduct,
                      coordProduct,      (y * y) / divider);
  }
  
  /**
    * @returns the identity matrix.
    */
  static const Matrix2<T>& identity();

  /**
    * @returns the a 90 degree rotation matrix.
    */
  static const Matrix2<T>& rotate90();

  /**
    * @returns the a 180 degree rotation matrix.
    */
  static const Matrix2<T>& rotate180();

  /**
    * @returns the a 270 degree rotation matrix.
    */
  static const Matrix2<T>& rotate270();

  /**
    * @returns a matrix reflecting along the x-axis.
    */
  static const Matrix2<T>& reflectX();

  /**
    * @returns a matrix reflecting along the y-axis.
    */
  static const Matrix2<T>& reflectY();

  /**
    * @returns the x unit vector.
    */
  static const Vector2<T>& xUnit();

  /**
    * @returns the y unit vector.
    */
  static const Vector2<T>& yUnit();
};

template<class T>
const Matrix2<T>& Matrix2<T>::identity() {
  static Matrix2<T> m(1, 0, 0, 1);
  return m;
}

template<class T>
const Matrix2<T>& Matrix2<T>::rotate90() {
  static Matrix2<T> m(0, -1, 1, 0);
  return m;
}

template<class T>
const Matrix2<T>& Matrix2<T>::rotate180() {
  static Matrix2<T> m(-1, 0, 0, -1);
  return m;
}

template<class T>
const Matrix2<T>& Matrix2<T>::rotate270() {
  static Matrix2<T> m(0, 1, -1, 0);
  return m;
}

template<class T>
const Matrix2<T>& Matrix2<T>::reflectX() {
  static Matrix2<T> m(-1, 0, 0, 1);
  return m;
}

template<class T>
const Matrix2<T>& Matrix2<T>::reflectY() {
  static Matrix2<T> m(1, 0, 0, -1);
  return m;
}

template<class T>
const Vector2<T>& Matrix2<T>::xUnit() {
  static Vector2<T> v(1, 0);
  return v;
}

template<class T>
const Vector2<T>& Matrix2<T>::yUnit() {
  static Vector2<T> v(0, 1);
  return v;
}

/**
  * Represents a three-dimensional matrix with component type T. This class
  * implements many important operations for matrixes. Many of those are
  * defined in the parent class. The operations defined in this class are
  * mostly specific to three-dimensional matrixes.
  * 
  * Use this class to transform absolute points in space, directional
  * vectors, or normals.
  * 
  * Construction of matrixes is as expected. The default constructor creates the
  * identity matrix, there is a component-wise constructor, and a generic copy
  * constructor that converts any matrix type to a three-dimensional matrix.
  * 
  * Additionally, there are a lot of special static construction methods, like
  * rotate(), or scale().
  *
  * @tparam T the coordinate type, usually a floating point type.
  */
template<class T>
class Matrix3 : public Matrix<3, T, Vector3<T>, Matrix3<T>> {
  typedef Matrix<3, T, Vector3<T>, Matrix3<T>> Base;
public:
  using Base::cell;
  using Base::setCell;
  using Base::col;
  
  /**
    * Default constructor. Constructs the identity matrix
    * 
    * \f[\left(\begin{array}{cccc}
    *   1 & 0 & 0 \\
    *   0 & 1 & 0 \\
    *   0 & 0 & 1 \\
    * \end{array}\right)\f]
    */
  inline constexpr Matrix3()
    : Base()
  {
  }

  /**
    * Constructs a matrix from source, which can be of a different Matrix type.
    * The source Matrix can be of different size, but only the top left 3x3
    * submatrix is copied. If the source is smaller, then the rest is filled
    * with zeroes, except for the diagonal.
    */
  template<int D, class V, class M>
  inline constexpr Matrix3(const Matrix<D, T, V, M>& source)
    : Base(source)
  {
  }

  /**
    * Component-wise constructor for a Matrix3.
    */
  inline constexpr Matrix3(const T& c00, const T& c01, const T& c02,
                            const T& c10, const T& c11, const T& c12,
                            const T& c20, const T& c21, const T& c22) {
    setCell(0, 0, c00); setCell(0, 1, c01); setCell(0, 2, c02);
    setCell(1, 0, c10); setCell(1, 1, c11); setCell(1, 2, c12);
    setCell(2, 0, c20); setCell(2, 1, c21); setCell(2, 2, c22);
  }

  /**
    * @returns a matrix that represents a rotation around the x axis with angle.
    */
  template<class A>
  [[nodiscard]] inline static Matrix3<T> rotateX(const A& angle) noexcept {
    T sin = std::sin(angle.radians()), cos = std::cos(angle.radians());
    return Matrix3<T>(T(1), T(), T(),
                      T(),  cos, -sin,
                      T(),  sin, cos);
  }

  /**
    * @returns a matrix that represents a rotation around the y axis with angle.
    */
  template<class A>
  [[nodiscard]] inline static Matrix3<T> rotateY(const A& angle) noexcept {
    T sin = std::sin(angle.radians()), cos = std::cos(angle.radians());
    return Matrix3<T>(cos,  T(),  sin,
                      T(),  T(1), T(),
                      -sin, T(),  cos);
  }

  /**
    * @returns a matrix that represents a rotation around the z axis with angle.
    */
  template<class A>
  [[nodiscard]] inline static Matrix3<T> rotateZ(const A& angle) noexcept {
    T sin = std::sin(angle.radians()), cos = std::cos(angle.radians());
    return Matrix3<T>(cos, -sin, T(),
                      sin, cos,  T(),
                      T(), T(),  T(1));
  }

  /**
    * @returns a matrix that represents a rotation around all axes with angles
    *   given in the angles Vector3.
    */
  [[nodiscard]] inline static Matrix3<T> rotate(const Vector3<T>& angles) noexcept {
    return rotate(
      Angle<T>::fromRadians(angles.x()),
      Angle<T>::fromRadians(angles.y()),
      Angle<T>::fromRadians(angles.z())
    );
  }

  /**
    * @returns a matrix that represents a rotation around all axes with angles
    *   x, y, and z.
    */
  template<class A>
  [[nodiscard]] inline static Matrix3<T> rotate(const A& x, const A& y, const A& z) noexcept {
    return rotateZ(z) * rotateY(y) * rotateX(x);
  }

  /**
    * @returns a matrix that represents a scaling of factor.
    */
  [[nodiscard]] inline static constexpr Matrix3<T> scale(const T& factor) noexcept {
    return Matrix3<T>(factor, T(),    T(),
                      T(),    factor, T(),
                      T(),    T(),    factor);
  }

  /**
    * @returns a matrix that represents a scaling of the x axis with xFactor,
    *   the y axis with yFactor, and the z axis with zFactor.
    */
  [[nodiscard]] inline static constexpr Matrix3<T> scale(const T& xFactor, const T& yFactor, const T& zFactor) noexcept {
    return Matrix3<T>(xFactor, T(),     T(),
                      T(),     yFactor, T(),
                      T(),     T(),     zFactor);
  }

  /**
    * @returns a matrix that represents a scaling of three axes given by the
    *   components of the factor Vector3.
    */
  [[nodiscard]] inline static constexpr Matrix3<T> scale(const Vector3<T>& factor) noexcept {
    return Matrix3<T>(factor[0], T(),       T(),
                      T(),       factor[1], T(),
                      T(),       T(),       factor[2]);
  }

  /**
    * @returns a vector containing the scaling factors in the matrix. This
    *   method assumes that the matrix was generated as a TRS matrix.
    */
  [[nodiscard]] inline Vector3<T> scaleVector() const noexcept {
    return Vector3<T>(
      col(0).length(),
      col(1).length(),
      col(2).length()
    );
  }

  /**
    * @returns a quaternion describing the same rotation as the matrix.
    */
  [[nodiscard]] inline Quaternion<T> rotationQuaternion() const;

  /**
    * @returns a vector with the three Euler angles that describe the same
    *   rotation as this matrix. This  method assumes that the matrix was
    *   generated as a TRS matrix.
    */
  [[nodiscard]] inline Vector3<T> rotationVector() const noexcept {
    T t1 = std::atan2(cell(2, 1), cell(2, 2));
    T c2 = std::sqrt(cell(0, 0) * cell(0, 0) + cell(1, 0) * cell(1, 0));
    T t2 = std::atan2(-cell(2, 0), c2);
    T s1 = std::sin(t1),
      c1 = std::cos(t1);
    T t3 = std::atan2(s1 * cell(0, 2) - c1 * cell(0, 1), c1 * cell(1, 1) - s1 * cell(1, 2));

    return Vector3<T>(t1, t2, t3);
  }
};

template<class T>
Quaternion<T> Matrix3<T>::rotationQuaternion() const {
  Vector3<T> s = scaleVector();

  double m00 = cell(0, 0) / s.x();
  double m01 = cell(0, 1) / s.y();
  double m02 = cell(0, 2) / s.z();
  double m10 = cell(1, 0) / s.x();
  double m11 = cell(1, 1) / s.y();
  double m12 = cell(1, 2) / s.z();
  double m20 = cell(2, 0) / s.x();
  double m21 = cell(2, 1) / s.y();
  double m22 = cell(2, 2) / s.z();

  double w = std::sqrt(std::max(T(0), T(1 + m00 + m11 + m22))) / 2;
  double x = std::sqrt(std::max(T(0), T(1 + m00 - m11 - m22))) / 2;
  double y = std::sqrt(std::max(T(0), T(1 - m00 + m11 - m22))) / 2;
  double z = std::sqrt(std::max(T(0), T(1 - m00 - m11 + m22))) / 2;
  
  x = std::copysign(x, T(x * (m21 - m12)));
  y = std::copysign(y, T(y * (m02 - m20)));
  z = std::copysign(z, T(z * (m10 - m01)));
  
  return Quaternion<T>(w, x, y, z).normalized();
}

/**
  * Represents a four-dimensional matrix with component type T. This class
  * implements many important operations for matrixes. Many of those are
  * defined in the parent class. The operations defined in this class are
  * mostly specific to four-dimensional matrixes.
  * 
  * Use this class to transform absolute points in space, directional  vectors,
  * or normals. Other than Matrix3, this class allows to describe translations.
  * 
  * Construction of matrixes is as expected. The default constructor creates the
  * identity matrix, there is a component-wise constructor, and a generic copy
  * constructor that converts any matrix type to a three-dimensional matrix.
  * 
  * Additionally, there are a few of special static construction methods, like
  * translate(). To construct four-dimensional matrixes for other operations,
  * like rotation, or scaling, construct a Matrix3 and convert it to Matrix4.
  *
  * @tparam T the coordinate type, usually a floating point type.
  */
template<class T>
class Matrix4 : public Matrix<4, T, Vector4<T>, Matrix4<T>> {
  typedef Matrix<4, T, Vector4<T>, Matrix4<T>> Base;
public:
  using Base::cell;
  using Base::setCell;
  using Base::col;
  // Re-introduce all base operator* overloads (scalar multiply, etc.) so
  // the two explicit declarations below don't hide them via name hiding.
  using Base::operator*;

  /**
    * @returns the matrix multiplication of this matrix and other.
    * Declared explicitly so the matrix/sse2/ headers can provide SIMD
    * specializations for float and double without specializing the whole class.
    */
  [[nodiscard]] inline Matrix4<T> operator*(const Matrix4<T>& other) const noexcept {
    return Base::operator*(other);
  }

  /**
    * @returns the Vector that is the result of multiplying this Matrix by vec.
    * Declared explicitly so the matrix/sse2/ headers can provide SIMD
    * specializations for float and double without specializing the whole class.
    */
  [[nodiscard]] inline Vector4<T> operator*(const Vector4<T>& vec) const noexcept {
    return Base::operator*(vec);
  }

  /**
    * Default constructor. Constructs the identity matrix
    * 
    * \f[\left(\begin{array}{cccc}
    *   1 & 0 & 0 & 0 \\
    *   0 & 1 & 0 & 0 \\
    *   0 & 0 & 1 & 0 \\
    *   0 & 0 & 0 & 1 \\
    * \end{array}\right)\f]
    */
  inline constexpr Matrix4()
    : Base()
  {
  }

  /**
    * Constructs a matrix from source, which can be of a different Matrix type.
    * The source Matrix can be of different size, but only the top left 4x4
    * submatrix is copied. If the source is smaller, then the rest is filled
    * with zeroes, except for the diagonal.
    */
  template<int D, class V, class M>
  inline constexpr Matrix4(const Matrix<D, T, V, M>& source)
    : Base(source)
  {
  }

  /**
    * Component-wise constructor for a Matrix4.
    */
  inline constexpr Matrix4(const T& c00, const T& c01, const T& c02, const T& c03,
                            const T& c10, const T& c11, const T& c12, const T& c13,
                            const T& c20, const T& c21, const T& c22, const T& c23,
                            const T& c30, const T& c31, const T& c32, const T& c33) {
    setCell(0, 0, c00); setCell(0, 1, c01); setCell(0, 2, c02); setCell(0, 3, c03);
    setCell(1, 0, c10); setCell(1, 1, c11); setCell(1, 2, c12); setCell(1, 3, c13);
    setCell(2, 0, c20); setCell(2, 1, c21); setCell(2, 2, c22); setCell(2, 3, c23);
    setCell(3, 0, c30); setCell(3, 1, c31); setCell(3, 2, c32); setCell(3, 3, c33);
  }

  /**
    * Constructs a matrix using three axis vectors xAxis, yAxis, and zAxis:
    *
    * \f[\left(\begin{array}{cccc}
    *   x_0 & x_1 & x_2 & 0 \\
    *   y_0 & y_1 & y_2 & 0 \\
    *   z_0 & z_1 & z_2 & 0 \\
    *   0   & 0   & 0   & 1 \\
    * \end{array}\right)\f]
    */
  inline constexpr Matrix4(const Vector3<T>& xAxis, const Vector3<T>& yAxis, const Vector3<T>& zAxis) {
    setCell(0, 0, xAxis[0]); setCell(0, 1, xAxis[1]); setCell(0, 2, xAxis[2]); setCell(0, 3, T());
    setCell(1, 0, yAxis[0]); setCell(1, 1, yAxis[1]); setCell(1, 2, yAxis[2]); setCell(1, 3, T());
    setCell(2, 0, zAxis[0]); setCell(2, 1, zAxis[1]); setCell(2, 2, zAxis[2]); setCell(2, 3, T());
    setCell(3, 0, T());      setCell(3, 1, T());      setCell(3, 2, T());      setCell(3, 3, T(1));
  }

  /**
    * @returns the inverse \f$M'\f$ of this matrix \f$M\f$, so that \f$M \times
    *   M' = I\f$, where \f$I\f$ is the identity matrix.
    */
  [[nodiscard]] Matrix4<T> inverted() const;

  /**
    * @returns a numerically stable inverse of this matrix. Well-conditioned
    *   matrices use the fast block inverse from inverted(); ill-conditioned
    *   matrices are recomputed with LU partial pivoting.
    */
  [[nodiscard]] Matrix4<T> stableInverse() const;

  /**
    * @returns the determinant \f$|M|\f$ of this matrix \f$M\f$.
    */
  [[nodiscard]] T determinant() const noexcept;
  
  /**
    * @returns the translation matrix for position \f$p\f$:
    * 
    * \f[\left(\begin{array}{cccc}
    *   1 & 0 & 0 & p_0 \\
    *   0 & 1 & 0 & p_1 \\
    *   0 & 0 & 1 & p_2 \\
    *   0 & 0 & 0 & 1 \\
    * \end{array}\right)\f]
    */
  [[nodiscard]] inline static constexpr Matrix4<T> translate(const Vector3<T>& position) noexcept {
    return translate(position.x(), position.y(), position.z());
  }

  /**
    * @returns the translation matrix for position \f$(x, y, z)\f$:
    *
    * \f[\left(\begin{array}{cccc}
    *   1 & 0 & 0 & x \\
    *   0 & 1 & 0 & y \\
    *   0 & 0 & 1 & z \\
    *   0 & 0 & 0 & 1 \\
    * \end{array}\right)\f]
    */
  [[nodiscard]] inline static constexpr Matrix4<T> translate(T x, T y, T z) noexcept {
    return Matrix4<T>(
      1, 0, 0, x,
      0, 1, 0, y,
      0, 0, 1, z,
      0, 0, 0, 1
    );
  }

  /**
    * @returns the translation vector \f$(x,y,z)\f$ extracted from the matrix:
    *
    * \f[\left(\begin{array}{cccc}
    *   c_{00} & c_{01} & c_{02} & x \\
    *   c_{10} & c_{11} & c_{12} & y \\
    *   c_{20} & c_{21} & c_{22} & z \\
    *   c_{30} & c_{31} & c_{32} & c_{33} \\
    * \end{array}\right)\f]
    */
  [[nodiscard]] inline constexpr Vector3<T> translationVector() const noexcept {
    return Vector3<T>(col(3));
  }

  /**
    * Transforms point p by this affine matrix, skipping the homogeneous bottom
    * row (assumed (0,0,0,1)) and returning a Vector3 directly.  Equivalent to
    * `((*this) * Vector4(p, 1)).xyz()` but computes only 12 multiply-adds
    * instead of 16 and avoids the perspective divide.
    *
    * Precondition: row 3 is (0,0,0,1). Results are undefined otherwise.
    */
  inline Vector3<T> transformPoint(const Vector3<T>& p) const {
    return Vector3<T>(
      cell(0,0)*p.x() + cell(0,1)*p.y() + cell(0,2)*p.z() + cell(0,3),
      cell(1,0)*p.x() + cell(1,1)*p.y() + cell(1,2)*p.z() + cell(1,3),
      cell(2,0)*p.x() + cell(2,1)*p.y() + cell(2,2)*p.z() + cell(2,3)
    );
  }

  /**
    * Transforms direction d by this affine matrix, skipping the translation
    * column and the homogeneous bottom row.  Equivalent to
    * `((*this) * Vector4(d, 0)).xyz()` but computes only 9 multiply-adds
    * instead of 16.
    *
    * Precondition: row 3 is (0,0,0,1). Results are undefined otherwise.
    */
  inline Vector3<T> transformDirection(const Vector3<T>& d) const {
    return Vector3<T>(
      cell(0,0)*d.x() + cell(0,1)*d.y() + cell(0,2)*d.z(),
      cell(1,0)*d.x() + cell(1,1)*d.y() + cell(1,2)*d.z(),
      cell(2,0)*d.x() + cell(2,1)*d.y() + cell(2,2)*d.z()
    );
  }

  /**
    * @returns the camera-to-world transform for a camera at eye looking toward
    *   target, with up as the world-space up hint. The resulting matrix maps
    *   camera-space axes to world space:
    *   - camera +Z (forward) → direction from eye to target
    *   - camera +X (right) → right-hand perpendicular of up and forward
    *   - camera +Y (up) → camera-space up direction
    *   - camera origin → eye position
    *
    *   Use inverted() on the result to obtain the view matrix (world-to-camera).
    *
    *   Degenerates when eye == target or when up is parallel to the
    *   eye-to-target direction (gimbal lock); callers must avoid these cases.
    */
  [[nodiscard]] inline static Matrix4<T> lookAt(const Vector3<T>& eye, const Vector3<T>& target, const Vector3<T>& up) {
    auto zAxis = (target - eye).normalized();
    auto xAxis = (up ^ zAxis).normalized();
    auto yAxis = xAxis ^ -zAxis;
    return Matrix4<T>(
      xAxis[0], yAxis[0], zAxis[0], eye[0],
      xAxis[1], yAxis[1], zAxis[1], eye[1],
      xAxis[2], yAxis[2], zAxis[2], eye[2],
      T(),      T(),      T(),      T(1)
    );
  }

  /**
    * @returns a symmetric perspective-projection matrix. The camera looks along
    *   +Z; z_ndc = -1 at the near plane and +1 at the far plane after the
    *   perspective divide by w (= z_eye).
    *
    *   @param fovY  vertical field-of-view angle
    *   @param aspect  width / height ratio
    *   @param nearPlane  positive near-plane distance
    *   @param farPlane   positive far-plane distance (farPlane > nearPlane)
    */
  template<class A>
  [[nodiscard]] inline static Matrix4<T> perspective(const A& fovY, T aspect, T nearPlane, T farPlane) noexcept {
    T f = T(1) / std::tan(fovY.radians() / T(2));
    T inv_range = T(1) / (farPlane - nearPlane);
    return Matrix4<T>(
      f / aspect, T(),  T(),                                T(),
      T(),        f,    T(),                                T(),
      T(),        T(),  (farPlane + nearPlane) * inv_range,  T(-2) * farPlane * nearPlane * inv_range,
      T(),        T(),  T(1),                               T()
    );
  }

  /**
    * @returns an orthographic-projection matrix that maps the axis-aligned box
    *   [left, right] × [bottom, top] × [nearPlane, farPlane] to NDC [-1, 1]³.
    *   No perspective divide; w = 1.
    */
  [[nodiscard]] inline static constexpr Matrix4<T> orthographic(T left, T right, T bottom, T top, T nearPlane, T farPlane) noexcept {
    T inv_rl = T(1) / (right - left);
    T inv_tb = T(1) / (top - bottom);
    T inv_fn = T(1) / (farPlane - nearPlane);
    return Matrix4<T>(
      T(2) * inv_rl, T(),           T(),           -(right + left) * inv_rl,
      T(),           T(2) * inv_tb, T(),           -(top + bottom) * inv_tb,
      T(),           T(),           T(2) * inv_fn, -(farPlane + nearPlane) * inv_fn,
      T(),           T(),           T(),            T(1)
    );
  }

  /**
    * @returns a general (asymmetric) perspective-projection matrix defined by
    *   near-plane corners (left, bottom) and (right, top). Identical to
    *   perspective() for the symmetric case where left = -right and
    *   bottom = -top. z_ndc = -1 at nearPlane, +1 at farPlane after dividing
    *   by w (= z_eye).
    */
  [[nodiscard]] inline static constexpr Matrix4<T> frustum(T left, T right, T bottom, T top, T nearPlane, T farPlane) noexcept {
    T inv_rl = T(1) / (right - left);
    T inv_tb = T(1) / (top - bottom);
    T inv_fn = T(1) / (farPlane - nearPlane);
    return Matrix4<T>(
      T(2) * nearPlane * inv_rl, T(),                      -(right + left) * inv_rl,        T(),
      T(),                       T(2) * nearPlane * inv_tb, -(top + bottom) * inv_tb,        T(),
      T(),                       T(),                        (farPlane + nearPlane) * inv_fn, T(-2) * farPlane * nearPlane * inv_fn,
      T(),                       T(),                        T(1),                            T()
    );
  }
};

template<class T>
Matrix4<T> Matrix4<T>::inverted() const {
  // Block-inverse via Schur complement: partition M into four 2×2 blocks
  //   M = | A  B |   M^{-1} = | S^{-1}             -S^{-1}·B·D^{-1}               |
  //       | C  D |             | -D^{-1}·C·S^{-1}   D^{-1}+D^{-1}·C·S^{-1}·B·D^{-1} |
  // where S = A - B·D^{-1}·C is the Schur complement of D in M.
  // det(S)==0 iff det(M)==0 (given det(D)!=0), so the second guard below is
  // the correct singular-matrix check for the full 4×4.
  // When det(D)==0 the block-inverse is inapplicable; fall back to cofactors.

  const T a00 = cell(0,0), a01 = cell(0,1), a10 = cell(1,0), a11 = cell(1,1);
  const T b00 = cell(0,2), b01 = cell(0,3), b10 = cell(1,2), b11 = cell(1,3);
  const T c00 = cell(2,0), c01 = cell(2,1), c10 = cell(3,0), c11 = cell(3,1);
  const T d00 = cell(2,2), d01 = cell(2,3), d10 = cell(3,2), d11 = cell(3,3);

  // D^{-1} (bottom-right 2×2 block)
  const T det_d = d00 * d11 - d01 * d10;
  if (det_d == T()) {
    // D is singular — fall back to adjugate / determinant (cofactor expansion).
    const T det = determinant();
    if (det == T())
      throw DivisionByZeroException(__FILE__, __LINE__);
    const T inv_det = T(1) / det;
    return Matrix4<T>(
      (cell(1,2)*cell(2,3)*cell(3,1)-cell(1,3)*cell(2,2)*cell(3,1)+cell(1,3)*cell(2,1)*cell(3,2)-cell(1,1)*cell(2,3)*cell(3,2)-cell(1,2)*cell(2,1)*cell(3,3)+cell(1,1)*cell(2,2)*cell(3,3))*inv_det,
      (cell(0,3)*cell(2,2)*cell(3,1)-cell(0,2)*cell(2,3)*cell(3,1)-cell(0,3)*cell(2,1)*cell(3,2)+cell(0,1)*cell(2,3)*cell(3,2)+cell(0,2)*cell(2,1)*cell(3,3)-cell(0,1)*cell(2,2)*cell(3,3))*inv_det,
      (cell(0,2)*cell(1,3)*cell(3,1)-cell(0,3)*cell(1,2)*cell(3,1)+cell(0,3)*cell(1,1)*cell(3,2)-cell(0,1)*cell(1,3)*cell(3,2)-cell(0,2)*cell(1,1)*cell(3,3)+cell(0,1)*cell(1,2)*cell(3,3))*inv_det,
      (cell(0,3)*cell(1,2)*cell(2,1)-cell(0,2)*cell(1,3)*cell(2,1)-cell(0,3)*cell(1,1)*cell(2,2)+cell(0,1)*cell(1,3)*cell(2,2)+cell(0,2)*cell(1,1)*cell(2,3)-cell(0,1)*cell(1,2)*cell(2,3))*inv_det,
      (cell(1,3)*cell(2,2)*cell(3,0)-cell(1,2)*cell(2,3)*cell(3,0)-cell(1,3)*cell(2,0)*cell(3,2)+cell(1,0)*cell(2,3)*cell(3,2)+cell(1,2)*cell(2,0)*cell(3,3)-cell(1,0)*cell(2,2)*cell(3,3))*inv_det,
      (cell(0,2)*cell(2,3)*cell(3,0)-cell(0,3)*cell(2,2)*cell(3,0)+cell(0,3)*cell(2,0)*cell(3,2)-cell(0,0)*cell(2,3)*cell(3,2)-cell(0,2)*cell(2,0)*cell(3,3)+cell(0,0)*cell(2,2)*cell(3,3))*inv_det,
      (cell(0,3)*cell(1,2)*cell(3,0)-cell(0,2)*cell(1,3)*cell(3,0)-cell(0,3)*cell(1,0)*cell(3,2)+cell(0,0)*cell(1,3)*cell(3,2)+cell(0,2)*cell(1,0)*cell(3,3)-cell(0,0)*cell(1,2)*cell(3,3))*inv_det,
      (cell(0,2)*cell(1,3)*cell(2,0)-cell(0,3)*cell(1,2)*cell(2,0)+cell(0,3)*cell(1,0)*cell(2,2)-cell(0,0)*cell(1,3)*cell(2,2)-cell(0,2)*cell(1,0)*cell(2,3)+cell(0,0)*cell(1,2)*cell(2,3))*inv_det,
      (cell(1,1)*cell(2,3)*cell(3,0)-cell(1,3)*cell(2,1)*cell(3,0)+cell(1,3)*cell(2,0)*cell(3,1)-cell(1,0)*cell(2,3)*cell(3,1)-cell(1,1)*cell(2,0)*cell(3,3)+cell(1,0)*cell(2,1)*cell(3,3))*inv_det,
      (cell(0,3)*cell(2,1)*cell(3,0)-cell(0,1)*cell(2,3)*cell(3,0)-cell(0,3)*cell(2,0)*cell(3,1)+cell(0,0)*cell(2,3)*cell(3,1)+cell(0,1)*cell(2,0)*cell(3,3)-cell(0,0)*cell(2,1)*cell(3,3))*inv_det,
      (cell(0,1)*cell(1,3)*cell(3,0)-cell(0,3)*cell(1,1)*cell(3,0)+cell(0,3)*cell(1,0)*cell(3,1)-cell(0,0)*cell(1,3)*cell(3,1)-cell(0,1)*cell(1,0)*cell(3,3)+cell(0,0)*cell(1,1)*cell(3,3))*inv_det,
      (cell(0,3)*cell(1,1)*cell(2,0)-cell(0,1)*cell(1,3)*cell(2,0)-cell(0,3)*cell(1,0)*cell(2,1)+cell(0,0)*cell(1,3)*cell(2,1)+cell(0,1)*cell(1,0)*cell(2,3)-cell(0,0)*cell(1,1)*cell(2,3))*inv_det,
      (cell(1,2)*cell(2,1)*cell(3,0)-cell(1,1)*cell(2,2)*cell(3,0)-cell(1,2)*cell(2,0)*cell(3,1)+cell(1,0)*cell(2,2)*cell(3,1)+cell(1,1)*cell(2,0)*cell(3,2)-cell(1,0)*cell(2,1)*cell(3,2))*inv_det,
      (cell(0,1)*cell(2,2)*cell(3,0)-cell(0,2)*cell(2,1)*cell(3,0)+cell(0,2)*cell(2,0)*cell(3,1)-cell(0,0)*cell(2,2)*cell(3,1)-cell(0,1)*cell(2,0)*cell(3,2)+cell(0,0)*cell(2,1)*cell(3,2))*inv_det,
      (cell(0,2)*cell(1,1)*cell(3,0)-cell(0,1)*cell(1,2)*cell(3,0)-cell(0,2)*cell(1,0)*cell(3,1)+cell(0,0)*cell(1,2)*cell(3,1)+cell(0,1)*cell(1,0)*cell(3,2)-cell(0,0)*cell(1,1)*cell(3,2))*inv_det,
      (cell(0,1)*cell(1,2)*cell(2,0)-cell(0,2)*cell(1,1)*cell(2,0)+cell(0,2)*cell(1,0)*cell(2,1)-cell(0,0)*cell(1,2)*cell(2,1)-cell(0,1)*cell(1,0)*cell(2,2)+cell(0,0)*cell(1,1)*cell(2,2))*inv_det
    );
  }

  const T inv_det_d = T(1) / det_d;
  const T di00 =  d11 * inv_det_d, di01 = -d01 * inv_det_d;
  const T di10 = -d10 * inv_det_d, di11 =  d00 * inv_det_d;

  // X = B·D^{-1}
  const T x00 = b00*di00 + b01*di10, x01 = b00*di01 + b01*di11;
  const T x10 = b10*di00 + b11*di10, x11 = b10*di01 + b11*di11;

  // S = A - X·C  (Schur complement)
  const T s00 = a00 - (x00*c00 + x01*c10), s01 = a01 - (x00*c01 + x01*c11);
  const T s10 = a10 - (x10*c00 + x11*c10), s11 = a11 - (x10*c01 + x11*c11);

  // S^{-1}
  const T det_s = s00 * s11 - s01 * s10;
  if (det_s == T())
    throw DivisionByZeroException(__FILE__, __LINE__);
  const T inv_det_s = T(1) / det_s;
  const T si00 =  s11 * inv_det_s, si01 = -s01 * inv_det_s;
  const T si10 = -s10 * inv_det_s, si11 =  s00 * inv_det_s;

  // Top-right block: -S^{-1}·X = -S^{-1}·B·D^{-1}
  const T tr00 = -(si00*x00 + si01*x10), tr01 = -(si00*x01 + si01*x11);
  const T tr10 = -(si10*x00 + si11*x10), tr11 = -(si10*x01 + si11*x11);

  // Q = D^{-1}·C·S^{-1}  (via Y = D^{-1}·C first)
  const T y00 = di00*c00 + di01*c10, y01 = di00*c01 + di01*c11;
  const T y10 = di10*c00 + di11*c10, y11 = di10*c01 + di11*c11;
  const T q00 = y00*si00 + y01*si10, q01 = y00*si01 + y01*si11;
  const T q10 = y10*si00 + y11*si10, q11 = y10*si01 + y11*si11;

  // Bottom-left: -Q;  Bottom-right: D^{-1} + Q·X
  return Matrix4<T>(
    si00, si01, tr00, tr01,
    si10, si11, tr10, tr11,
    -q00, -q01, di00 + q00*x00 + q01*x10, di01 + q00*x01 + q01*x11,
    -q10, -q11, di10 + q10*x00 + q11*x10, di11 + q10*x01 + q11*x11
  );
}

template<class T>
T Matrix4<T>::determinant() const noexcept {
  return cell(0, 3) * cell(1, 2) * cell(2, 1) * cell(3, 0)-cell(0, 2) * cell(1, 3) * cell(2, 1) * cell(3, 0)-cell(0, 3) * cell(1, 1) * cell(2, 2) * cell(3, 0)+cell(0, 1) * cell(1, 3) * cell(2, 2) * cell(3, 0) +
         cell(0, 2) * cell(1, 1) * cell(2, 3) * cell(3, 0)-cell(0, 1) * cell(1, 2) * cell(2, 3) * cell(3, 0)-cell(0, 3) * cell(1, 2) * cell(2, 0) * cell(3, 1)+cell(0, 2) * cell(1, 3) * cell(2, 0) * cell(3, 1) +
         cell(0, 3) * cell(1, 0) * cell(2, 2) * cell(3, 1)-cell(0, 0) * cell(1, 3) * cell(2, 2) * cell(3, 1)-cell(0, 2) * cell(1, 0) * cell(2, 3) * cell(3, 1)+cell(0, 0) * cell(1, 2) * cell(2, 3) * cell(3, 1) +
         cell(0, 3) * cell(1, 1) * cell(2, 0) * cell(3, 2)-cell(0, 1) * cell(1, 3) * cell(2, 0) * cell(3, 2)-cell(0, 3) * cell(1, 0) * cell(2, 1) * cell(3, 2)+cell(0, 0) * cell(1, 3) * cell(2, 1) * cell(3, 2) +
         cell(0, 1) * cell(1, 0) * cell(2, 3) * cell(3, 2)-cell(0, 0) * cell(1, 1) * cell(2, 3) * cell(3, 2)-cell(0, 2) * cell(1, 1) * cell(2, 0) * cell(3, 3)+cell(0, 1) * cell(1, 2) * cell(2, 0) * cell(3, 3) +
         cell(0, 2) * cell(1, 0) * cell(2, 1) * cell(3, 3)-cell(0, 0) * cell(1, 2) * cell(2, 1) * cell(3, 3)-cell(0, 1) * cell(1, 0) * cell(2, 2) * cell(3, 3)+cell(0, 0) * cell(1, 1) * cell(2, 2) * cell(3, 3);
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

// SIMD specializations of Matrix4<float> and Matrix4<double> operator* and
// operator*(Vector4).  Must come after the Matrix4<T> class definition but
// before the typedef aliases, mirroring the Vector.h / vector/sse3/ pattern.
#include "core/math/matrix/sse2/Matrix4f.h"
#include "core/math/matrix/sse2/Matrix4d.h"

/**
  * Two-dimensional matrix with float components.
  */
typedef Matrix2<float> Matrix2f;

/**
  * Two-dimensional matrix with double components.
  */
typedef Matrix2<double> Matrix2d;

/**
  * Three-dimensional matrix with float components.
  */
typedef Matrix3<float> Matrix3f;

/**
  * Three-dimensional matrix with double components.
  */
typedef Matrix3<double> Matrix3d;

/**
  * Four-dimensional matrix with float components.
  */
typedef Matrix4<float> Matrix4f;

/**
  * Four-dimensional matrix with double components.
  */
typedef Matrix4<double> Matrix4d;

// ---------------------------------------------------------------------------
// std::hash specializations — enables unordered_map/unordered_set keys.
// ---------------------------------------------------------------------------
namespace std {  // NOLINT(cert-dcl58-cpp) — extending std for UDTs is allowed

  template<int Dimensions, class T, class VectorType, class Derived>
  struct hash<Matrix<Dimensions, T, VectorType, Derived>> {
    size_t operator()(const Matrix<Dimensions, T, VectorType, Derived>& m) const noexcept {
      size_t seed = 0;
      hash<T> h;
      for (int row = 0; row < Dimensions; ++row)
        for (int col = 0; col < Dimensions; ++col)
          seed ^= h(m[row][col]) + size_t(0x9e3779b9) + (seed << 6) + (seed >> 2);
      return seed;
    }
  };

  template<class T>
  struct hash<Matrix2<T>> {
    size_t operator()(const Matrix2<T>& m) const noexcept {
      return hash<Matrix<2, T, Vector2<T>, Matrix2<T>>>{}(m);
    }
  };

  template<class T>
  struct hash<Matrix3<T>> {
    size_t operator()(const Matrix3<T>& m) const noexcept {
      return hash<Matrix<3, T, Vector3<T>, Matrix3<T>>>{}(m);
    }
  };

  template<class T>
  struct hash<Matrix4<T>> {
    size_t operator()(const Matrix4<T>& m) const noexcept {
      return hash<Matrix<4, T, Vector4<T>, Matrix4<T>>>{}(m);
    }
  };
}

// ---------------------------------------------------------------------------
// std::formatter specializations (C++20). Fallback: the operator<< above.
// ---------------------------------------------------------------------------
#ifdef __cpp_lib_format

template<int Dimensions, class T, class VectorType, class Derived>
struct std::formatter<Matrix<Dimensions, T, VectorType, Derived>> {  // NOLINT(cert-dcl58-cpp)
  constexpr auto parse(std::format_parse_context& ctx) const { return ctx.begin(); }
  auto format(const Matrix<Dimensions, T, VectorType, Derived>& m, std::format_context& ctx) const {
    auto out = ctx.out();
    for (int row = 0; row < Dimensions; ++row) {
      for (int col = 0; col < Dimensions; ++col) {
        if (col > 0) out = std::format_to(out, " ");
        out = std::format_to(out, "{}", m[row][col]);
      }
      out = std::format_to(out, "\n");
    }
    return out;
  }
};

template<class T>
struct std::formatter<Matrix2<T>> : std::formatter<Matrix<2, T, Vector2<T>, Matrix2<T>>> {};  // NOLINT(cert-dcl58-cpp)

template<class T>
struct std::formatter<Matrix3<T>> : std::formatter<Matrix<3, T, Vector3<T>, Matrix3<T>>> {};  // NOLINT(cert-dcl58-cpp)

template<class T>
struct std::formatter<Matrix4<T>> : std::formatter<Matrix<4, T, Vector4<T>, Matrix4<T>>> {};  // NOLINT(cert-dcl58-cpp)

#endif  // __cpp_lib_format
