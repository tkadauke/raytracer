#pragma once

#include <iostream>
#include <cmath>
#include "core/math/Vector.h"
#include "core/math/Angle.h"
#include "core/DivisionByZeroException.h"

template<int Dimensions, class T>
class Matrix {
  typedef T RowType[Dimensions];
  typedef RowType CellsType[Dimensions];
public:
  inline Matrix() {
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

  inline Matrix(const CellsType& cells) {
    for (int row = 0; row != Dimensions; ++row) {
      for (int col = 0; col != Dimensions; ++col) {
        m_cells[row][col] = cells[row][col];
      }
    }
  }
  
  template<int D>
  inline Matrix(const Matrix<D, T>& source) {
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

  inline const T& cell(int row, int col) const {
    return m_cells[row][col];
  }

  inline void setCell(int row, int col, const T& value) {
    m_cells[row][col] = value;
  }

  inline const RowType& operator[](int index) const {
    return m_cells[index];
  }

  inline RowType& operator[](int index) {
    return m_cells[index];
  }

  inline bool operator==(const Matrix<Dimensions, T>& other) const {
    for (int row = 0; row != Dimensions; ++row) {
      for (int col = 0; col != Dimensions; ++col) {
        if (m_cells[row][col] != other[row][col])
          return false;
      }
    }
    return true;
  }

  inline bool operator!=(const Matrix<Dimensions, T>& other) const {
    return !(*this == other);
  }

  inline Matrix<Dimensions, T> operator*(const Matrix<Dimensions, T>& other) const {
    Matrix<Dimensions, T> result;

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

  template<class StorageCellType, class Derived>
  inline Vector<Dimensions, T> operator*(const Vector<Dimensions, T, StorageCellType, Derived>& vector) const {
    Vector<Dimensions, T> result;

    for (int row = 0; row != Dimensions; ++row) {
      T cell = T();
      for (int i = 0; i != Dimensions; ++i) {
        cell += m_cells[row][i] * vector[i];
      }
      result[row] = cell;
    }
    return result;
  }

  inline Matrix<Dimensions, T> operator*(const T& scalar) const {
    Matrix<Dimensions, T> result;

    for (int row = 0; row != Dimensions; ++row) {
      for (int col = 0; col != Dimensions; ++col) {
        result[row][col] = m_cells[row][col] * scalar;
      }
    }
    return result;
  }

  inline Matrix<Dimensions, T> operator+(const Matrix<Dimensions, T>& other) const {
    Matrix<Dimensions, T> result;

    for (int row = 0; row != Dimensions; ++row) {
      for (int col = 0; col != Dimensions; ++col) {
        result[row][col] = m_cells[row][col] + other.cell(row, col);
      }
    }
    return result;
  }

  inline Matrix<Dimensions, T> operator/(const T& scalar) const {
    if (scalar == T())
      throw DivisionByZeroException(__FILE__, __LINE__);

    Matrix<Dimensions, T> result;

    for (int row = 0; row != Dimensions; ++row) {
      for (int col = 0; col != Dimensions; ++col) {
        result[row][col] = m_cells[row][col] / scalar;
      }
    }
    return result;
  }

  inline T rowSum(int row) const {
    T result = T();
    for (int col = 0; col != Dimensions; ++col) {
      result += m_cells[row][col];
    }
    return result;
  }

  inline T colSum(int col) const {
    T result = T();
    for (int row = 0; row != Dimensions; ++row) {
      result += m_cells[row][col];
    }
    return result;
  }
  
  inline Matrix<Dimensions, T> transposed() const {
    Matrix<Dimensions, T> result(*this);
    
    for (int row = 0; row != Dimensions; ++row) {
      for (int col = row + 1; col != Dimensions; ++col) {
        std::swap(result[row][col], result[col][row]);
      }
    }
    return result;
  }

private:
  CellsType m_cells;
};

template<int Dimensions, class T>
std::ostream& operator<<(std::ostream& os, const Matrix<Dimensions, T>& matrix) {
  for (int row = 0; row != Dimensions; ++row) {
    for (int col = 0; col != Dimensions; ++col) {
      os << matrix[row][col] << ' ';
    }
    os << std::endl;
  }
  return os;
}

template<int Dimensions, class T, class MatrixType, class VectorType>
class SpecializedMatrix : public Matrix<Dimensions, T> {
  typedef Matrix<Dimensions, T> Base;
public:
  typedef VectorType Vector;
  
  inline SpecializedMatrix()
    : Base()
  {
  }
  
  template<int D>
  inline SpecializedMatrix(const Matrix<D, T>& source)
    : Base(source)
  {
  }
  
  inline MatrixType operator*(const MatrixType& other) const {
    return static_cast<MatrixType>(this->Base::operator*(static_cast<MatrixType>(other)));
  }

  template<class StorageCellType, class Derived>
  inline VectorType operator*(const ::Vector<Dimensions, T, StorageCellType, Derived>& vector) const {
    return static_cast<VectorType>(this->Base::operator*(vector));
  }
  
  inline MatrixType operator*(const T& scalar) const {
    return static_cast<MatrixType>(this->Base::operator*(scalar));
  }

  inline MatrixType operator/(const T& scalar) const {
    return static_cast<MatrixType>(this->Base::operator/(scalar));
  }
  
  inline MatrixType transposed() const {
    return static_cast<MatrixType>(this->Base::transposed());
  }
};

template<class T>
class Matrix2 : public SpecializedMatrix<2, T, Matrix2<T>, Vector2<T>> {
  typedef SpecializedMatrix<2, T, Matrix2<T>, Vector2<T>> Base;
public:
  using Base::cell;
  using Base::setCell;
  
  inline Matrix2()
    : Base()
  {
  }

  template<int D>
  inline Matrix2(const Matrix<D, T>& source)
    : Base(source)
  {
  }

  inline Matrix2(const T& c00, const T& c01,
                 const T& c10, const T& c11) {
    setCell(0, 0, c00); setCell(0, 1, c01);
    setCell(1, 0, c10); setCell(1, 1, c11);
  }
  
  inline T determinant() const {
    return cell(0, 0) * cell(1, 1) - cell(0, 1) * cell(1, 0);
  }
  
  inline Matrix2<T> inverted() const {
    return Matrix2<T>(cell(1, 1), -cell(0, 1), -cell(1, 0), cell(0, 0)) / determinant();
  }
  
  template<class A>
  inline static Matrix2<T> rotate(const A& angle) {
    T sin = std::sin(angle.radians()), cos = std::cos(angle.radians());
    return Matrix2<T>(cos, -sin,
                      sin, cos);
  }
  
  template<class A>
  inline static Matrix2<T> clockwise(const A& angle) {
    return rotate(-angle);
  }
  
  template<class A>
  inline static Matrix2<T> counterclockwise(const A& angle) {
    return rotate(angle);
  }
  
  inline static Matrix2<T> scale(const T& factor) {
    return Matrix2<T>(factor, 0,
                      0,      factor);
  }

  inline static Matrix2<T> scale(const T& xFactor, const T& yFactor) {
    return Matrix2<T>(xFactor, 0,
                      0,       yFactor);
  }

  inline static Matrix2<T> shear(const T& xShear, const T& yShear) {
    return Matrix2<T>(1,      xShear,
                      yShear, 1);
  }

  inline static Matrix2<T> reflect(const Vector2<T>& vector) {
    return reflect(vector.x(), vector.y());
  }

  inline static Matrix2<T> reflect(const T& x, const T& y) {
    T len = std::sqrt(x * x + y * y);
    T divider = len*len;
    
    T coordProduct = (2 * x * y) / divider;
    
    return Matrix2<T>((x*x - y*y) / divider, coordProduct,
                      coordProduct,          (y*y - x*x) / divider);
  }

  inline static Matrix2<T> project(const Vector2<T>& vector) {
    return project(vector.x(), vector.y());
  }

  inline static Matrix2<T> project(const T& x, const T& y) {
    T len = std::sqrt(x * x + y * y);
    T divider = len*len;
    
    T coordProduct = (x * y) / divider;
    
    return Matrix2<T>((x * x) / divider, coordProduct,
                      coordProduct,      (y * y) / divider);
  }
  
  static const Matrix2<T>& identity();
  static const Matrix2<T>& rotate90();
  static const Matrix2<T>& rotate180();
  static const Matrix2<T>& rotate270();

  static const Matrix2<T>& reflectX();
  static const Matrix2<T>& reflectY();

  static const Vector2<T>& xUnit();
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

typedef Matrix2<float> Matrix2f;
typedef Matrix2<double> Matrix2d;

template<class T>
class Matrix3 : public SpecializedMatrix<3, T, Matrix3<T>, Vector3<T>> {
  typedef SpecializedMatrix<3, T, Matrix3<T>, Vector3<T>> Base;
public:
  using Base::setCell;
  
  inline Matrix3()
    : Base()
  {
  }

  template<int D>
  inline Matrix3(const Matrix<D, T>& source)
    : Base(source)
  {
  }

  inline Matrix3(const T& c00, const T& c01, const T& c02,
                 const T& c10, const T& c11, const T& c12,
                 const T& c20, const T& c21, const T& c22) {
    setCell(0, 0, c00); setCell(0, 1, c01); setCell(0, 2, c02);
    setCell(1, 0, c10); setCell(1, 1, c11); setCell(1, 2, c12);
    setCell(2, 0, c20); setCell(2, 1, c21); setCell(2, 2, c22);
  }
  
  template<class A>
  inline static Matrix3<T> rotateX(const A& angle) {
    T sin = std::sin(angle.radians()), cos = std::cos(angle.radians());
    return Matrix3<T>(T(1), T(), T(),
                      T(),  cos, -sin,
                      T(),  sin, cos);
  }
  
  template<class A>
  inline static Matrix3<T> rotateY(const A& angle) {
    T sin = std::sin(angle.radians()), cos = std::cos(angle.radians());
    return Matrix3<T>(cos,  T(),  sin,
                      T(),  T(1), T(),
                      -sin, T(),  cos);
  }
  
  template<class A>
  inline static Matrix3<T> rotateZ(const A& angle) {
    T sin = std::sin(angle.radians()), cos = std::cos(angle.radians());
    return Matrix3<T>(cos, -sin, T(),
                      sin, cos,  T(),
                      T(), T(),  T(1));
  }
  
  inline static Matrix3<T> rotate(const Vector3<T>& angles) {
    return rotate(
      Angle<T>::fromRadians(angles.x()),
      Angle<T>::fromRadians(angles.y()),
      Angle<T>::fromRadians(angles.z())
    );
  }
  
  template<class A>
  inline static Matrix3<T> rotate(const A& x, const A& y, const A& z) {
    return rotateX(x) * rotateY(y) * rotateZ(z);
  }
  
  inline static Matrix3<T> scale(const T& factor) {
    return Matrix3<T>(factor, T(),    T(),
                      T(),    factor, T(),
                      T(),    T(),    factor);
  }

  inline static Matrix3<T> scale(const T& xFactor, const T& yFactor, const T& zFactor) {
    return Matrix3<T>(xFactor, T(),     T(),
                      T(),     yFactor, T(),
                      T(),     T(),     zFactor);
  }

  inline static Matrix3<T> scale(const Vector3<T>& factor) {
    return Matrix3<T>(factor[0], T(),       T(),
                      T(),       factor[1], T(),
                      T(),       T(),       factor[2]);
  }
};

typedef Matrix3<float> Matrix3f;
typedef Matrix3<double> Matrix3d;

template<class T>
class Matrix4 : public SpecializedMatrix<4, T, Matrix4<T>, Vector4<T>> {
  typedef SpecializedMatrix<4, T, Matrix4<T>, Vector4<T>> Base;
public:
  using Base::cell;
  using Base::setCell;
  
  inline Matrix4()
    : Base()
  {
  }
  
  template<int D>
  inline Matrix4(const Matrix<D, T>& source)
    : Base(source)
  {
  }

  inline Matrix4(const T& c00, const T& c01, const T& c02, const T& c03,
                 const T& c10, const T& c11, const T& c12, const T& c13,
                 const T& c20, const T& c21, const T& c22, const T& c23,
                 const T& c30, const T& c31, const T& c32, const T& c33) {
    setCell(0, 0, c00); setCell(0, 1, c01); setCell(0, 2, c02); setCell(0, 3, c03);
    setCell(1, 0, c10); setCell(1, 1, c11); setCell(1, 2, c12); setCell(1, 3, c13);
    setCell(2, 0, c20); setCell(2, 1, c21); setCell(2, 2, c22); setCell(2, 3, c23);
    setCell(3, 0, c30); setCell(3, 1, c31); setCell(3, 2, c32); setCell(3, 3, c33);
  }

  inline Matrix4(const Vector3<T>& xAxis, const Vector3<T>& yAxis, const Vector3<T>& zAxis) {
    setCell(0, 0, xAxis[0]); setCell(0, 1, xAxis[1]); setCell(0, 2, xAxis[2]); setCell(0, 3, T());
    setCell(1, 0, yAxis[0]); setCell(1, 1, yAxis[1]); setCell(1, 2, yAxis[2]); setCell(1, 3, T());
    setCell(2, 0, zAxis[0]); setCell(2, 1, zAxis[1]); setCell(2, 2, zAxis[2]); setCell(2, 3, T());
    setCell(3, 0, T());      setCell(3, 1, T());      setCell(3, 2, T());      setCell(3, 3, T(1));
  }
  
  Matrix4<T> inverted() const;
  T determinant() const;
  
  inline static Matrix4<T> translate(const Vector3<T>& position) {
    return Matrix4<T>(
      1, 0, 0, position[0],
      0, 1, 0, position[1],
      0, 0, 1, position[2],
      0, 0, 0, 1
    );
  }
  
  inline Vector3<T> translation() const {
    return Vector3<T>(cell(0, 3), cell(1, 3), cell(2, 3));
  }
};

template<class T>
Matrix4<T> Matrix4<T>::inverted() const {
  return Matrix4<T>(
    cell(1, 2)*cell(2, 3)*cell(3, 1) - cell(1, 3)*cell(2, 2)*cell(3, 1) + cell(1, 3)*cell(2, 1)*cell(3, 2) - cell(1, 1)*cell(2, 3)*cell(3, 2) - cell(1, 2)*cell(2, 1)*cell(3, 3) + cell(1, 1)*cell(2, 2)*cell(3, 3),
    cell(0, 3)*cell(2, 2)*cell(3, 1) - cell(0, 2)*cell(2, 3)*cell(3, 1) - cell(0, 3)*cell(2, 1)*cell(3, 2) + cell(0, 1)*cell(2, 3)*cell(3, 2) + cell(0, 2)*cell(2, 1)*cell(3, 3) - cell(0, 1)*cell(2, 2)*cell(3, 3),
    cell(0, 2)*cell(1, 3)*cell(3, 1) - cell(0, 3)*cell(1, 2)*cell(3, 1) + cell(0, 3)*cell(1, 1)*cell(3, 2) - cell(0, 1)*cell(1, 3)*cell(3, 2) - cell(0, 2)*cell(1, 1)*cell(3, 3) + cell(0, 1)*cell(1, 2)*cell(3, 3),
    cell(0, 3)*cell(1, 2)*cell(2, 1) - cell(0, 2)*cell(1, 3)*cell(2, 1) - cell(0, 3)*cell(1, 1)*cell(2, 2) + cell(0, 1)*cell(1, 3)*cell(2, 2) + cell(0, 2)*cell(1, 1)*cell(2, 3) - cell(0, 1)*cell(1, 2)*cell(2, 3),
    cell(1, 3)*cell(2, 2)*cell(3, 0) - cell(1, 2)*cell(2, 3)*cell(3, 0) - cell(1, 3)*cell(2, 0)*cell(3, 2) + cell(1, 0)*cell(2, 3)*cell(3, 2) + cell(1, 2)*cell(2, 0)*cell(3, 3) - cell(1, 0)*cell(2, 2)*cell(3, 3),
    cell(0, 2)*cell(2, 3)*cell(3, 0) - cell(0, 3)*cell(2, 2)*cell(3, 0) + cell(0, 3)*cell(2, 0)*cell(3, 2) - cell(0, 0)*cell(2, 3)*cell(3, 2) - cell(0, 2)*cell(2, 0)*cell(3, 3) + cell(0, 0)*cell(2, 2)*cell(3, 3),
    cell(0, 3)*cell(1, 2)*cell(3, 0) - cell(0, 2)*cell(1, 3)*cell(3, 0) - cell(0, 3)*cell(1, 0)*cell(3, 2) + cell(0, 0)*cell(1, 3)*cell(3, 2) + cell(0, 2)*cell(1, 0)*cell(3, 3) - cell(0, 0)*cell(1, 2)*cell(3, 3),
    cell(0, 2)*cell(1, 3)*cell(2, 0) - cell(0, 3)*cell(1, 2)*cell(2, 0) + cell(0, 3)*cell(1, 0)*cell(2, 2) - cell(0, 0)*cell(1, 3)*cell(2, 2) - cell(0, 2)*cell(1, 0)*cell(2, 3) + cell(0, 0)*cell(1, 2)*cell(2, 3),
    cell(1, 1)*cell(2, 3)*cell(3, 0) - cell(1, 3)*cell(2, 1)*cell(3, 0) + cell(1, 3)*cell(2, 0)*cell(3, 1) - cell(1, 0)*cell(2, 3)*cell(3, 1) - cell(1, 1)*cell(2, 0)*cell(3, 3) + cell(1, 0)*cell(2, 1)*cell(3, 3),
    cell(0, 3)*cell(2, 1)*cell(3, 0) - cell(0, 1)*cell(2, 3)*cell(3, 0) - cell(0, 3)*cell(2, 0)*cell(3, 1) + cell(0, 0)*cell(2, 3)*cell(3, 1) + cell(0, 1)*cell(2, 0)*cell(3, 3) - cell(0, 0)*cell(2, 1)*cell(3, 3),
    cell(0, 1)*cell(1, 3)*cell(3, 0) - cell(0, 3)*cell(1, 1)*cell(3, 0) + cell(0, 3)*cell(1, 0)*cell(3, 1) - cell(0, 0)*cell(1, 3)*cell(3, 1) - cell(0, 1)*cell(1, 0)*cell(3, 3) + cell(0, 0)*cell(1, 1)*cell(3, 3),
    cell(0, 3)*cell(1, 1)*cell(2, 0) - cell(0, 1)*cell(1, 3)*cell(2, 0) - cell(0, 3)*cell(1, 0)*cell(2, 1) + cell(0, 0)*cell(1, 3)*cell(2, 1) + cell(0, 1)*cell(1, 0)*cell(2, 3) - cell(0, 0)*cell(1, 1)*cell(2, 3),
    cell(1, 2)*cell(2, 1)*cell(3, 0) - cell(1, 1)*cell(2, 2)*cell(3, 0) - cell(1, 2)*cell(2, 0)*cell(3, 1) + cell(1, 0)*cell(2, 2)*cell(3, 1) + cell(1, 1)*cell(2, 0)*cell(3, 2) - cell(1, 0)*cell(2, 1)*cell(3, 2),
    cell(0, 1)*cell(2, 2)*cell(3, 0) - cell(0, 2)*cell(2, 1)*cell(3, 0) + cell(0, 2)*cell(2, 0)*cell(3, 1) - cell(0, 0)*cell(2, 2)*cell(3, 1) - cell(0, 1)*cell(2, 0)*cell(3, 2) + cell(0, 0)*cell(2, 1)*cell(3, 2),
    cell(0, 2)*cell(1, 1)*cell(3, 0) - cell(0, 1)*cell(1, 2)*cell(3, 0) - cell(0, 2)*cell(1, 0)*cell(3, 1) + cell(0, 0)*cell(1, 2)*cell(3, 1) + cell(0, 1)*cell(1, 0)*cell(3, 2) - cell(0, 0)*cell(1, 1)*cell(3, 2),
    cell(0, 1)*cell(1, 2)*cell(2, 0) - cell(0, 2)*cell(1, 1)*cell(2, 0) + cell(0, 2)*cell(1, 0)*cell(2, 1) - cell(0, 0)*cell(1, 2)*cell(2, 1) - cell(0, 1)*cell(1, 0)*cell(2, 2) + cell(0, 0)*cell(1, 1)*cell(2, 2)
  ) * 1 / determinant();
}

template<class T>
T Matrix4<T>::determinant() const {
  return cell(0, 3) * cell(1, 2) * cell(2, 1) * cell(3, 0)-cell(0, 2) * cell(1, 3) * cell(2, 1) * cell(3, 0)-cell(0, 3) * cell(1, 1) * cell(2, 2) * cell(3, 0)+cell(0, 1) * cell(1, 3) * cell(2, 2) * cell(3, 0) +
         cell(0, 2) * cell(1, 1) * cell(2, 3) * cell(3, 0)-cell(0, 1) * cell(1, 2) * cell(2, 3) * cell(3, 0)-cell(0, 3) * cell(1, 2) * cell(2, 0) * cell(3, 1)+cell(0, 2) * cell(1, 3) * cell(2, 0) * cell(3, 1) +
         cell(0, 3) * cell(1, 0) * cell(2, 2) * cell(3, 1)-cell(0, 0) * cell(1, 3) * cell(2, 2) * cell(3, 1)-cell(0, 2) * cell(1, 0) * cell(2, 3) * cell(3, 1)+cell(0, 0) * cell(1, 2) * cell(2, 3) * cell(3, 1) +
         cell(0, 3) * cell(1, 1) * cell(2, 0) * cell(3, 2)-cell(0, 1) * cell(1, 3) * cell(2, 0) * cell(3, 2)-cell(0, 3) * cell(1, 0) * cell(2, 1) * cell(3, 2)+cell(0, 0) * cell(1, 3) * cell(2, 1) * cell(3, 2) +
         cell(0, 1) * cell(1, 0) * cell(2, 3) * cell(3, 2)-cell(0, 0) * cell(1, 1) * cell(2, 3) * cell(3, 2)-cell(0, 2) * cell(1, 1) * cell(2, 0) * cell(3, 3)+cell(0, 1) * cell(1, 2) * cell(2, 0) * cell(3, 3) +
         cell(0, 2) * cell(1, 0) * cell(2, 1) * cell(3, 3)-cell(0, 0) * cell(1, 2) * cell(2, 1) * cell(3, 3)-cell(0, 1) * cell(1, 0) * cell(2, 2) * cell(3, 3)+cell(0, 0) * cell(1, 1) * cell(2, 2) * cell(3, 3);
}

typedef Matrix4<float> Matrix4f;
typedef Matrix4<double> Matrix4d;
