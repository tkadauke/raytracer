#ifndef MATRIX_H
#define MATRIX_H

#include <iostream>
#include <cmath>
#include "Vector.h"
#include "DivisionByZeroException.h"

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

  inline Vector<Dimensions, T> operator*(const Vector<Dimensions, T>& vector) {
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
  
  inline Matrix<Dimensions, T> operator*(const T& scalar) {
    Matrix<Dimensions, T> result;

    for (int row = 0; row != Dimensions; ++row) {
      for (int col = 0; col != Dimensions; ++col) {
        result[row][col] = m_cells[row][col] * scalar;
      }
    }
    return result;
  }

  inline Matrix<Dimensions, T> operator/(const T& scalar) {
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

  inline T rowSum(int row) {
    T result = T();
    for (int col = 0; col != Dimensions; ++col) {
      result += m_cells[row][col];
    }
    return result;
  }

  inline T colSum(int col) {
    T result = T();
    for (int row = 0; row != Dimensions; ++row) {
      result += m_cells[row][col];
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

  inline VectorType operator*(const VectorType& vector) {
    return static_cast<VectorType>(this->Base::operator*(vector));
  }
  
  inline MatrixType operator*(const T& scalar) {
    return static_cast<MatrixType>(this->Base::operator*(scalar));
  }

  inline MatrixType operator/(const T& scalar) {
    return static_cast<MatrixType>(this->Base::operator/(scalar));
  }
};

template<class T>
class Matrix2 : public SpecializedMatrix<2, T, Matrix2<T>, Vector2<T> > {
  typedef SpecializedMatrix<2, T, Matrix2<T>, Vector2<T> > Base;
public:
  using Base::cell;
  
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
  
  inline static Matrix2<T> rotate(const T& angle) {
    T sin = std::sin(angle), cos = std::cos(angle);
    return Matrix2<T>(cos, -sin,
                      sin, cos);
  }
  
  inline static Matrix2<T> clockwise(const T& angle) {
    return rotate(-angle);
  }
  
  inline static Matrix2<T> counterclockwise(const T& angle) {
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
  
  static const Matrix2<T> identity;
  static const Matrix2<T> rotate90, rotate180, rotate270;
  
  static const Matrix2<T> reflectX, reflectY;
  
  static const Vector2<T> xUnit, yUnit;
};

template<class T>
const Matrix2<T> Matrix2<T>::identity = Matrix2<T>();

template<class T>
const Matrix2<T> Matrix2<T>::rotate90 = Matrix2<T>(0, -1, 1, 0);

template<class T>
const Matrix2<T> Matrix2<T>::rotate180 = Matrix2<T>(-1, 0, 0, -1);

template<class T>
const Matrix2<T> Matrix2<T>::rotate270 = Matrix2<T>(0, 1, -1, 0);

template<class T>
const Matrix2<T> Matrix2<T>::reflectX = Matrix2<T>(-1, 0, 0, 1);

template<class T>
const Matrix2<T> Matrix2<T>::reflectY = Matrix2<T>(1, 0, 0, -1);

template<class T>
const Vector2<T> Matrix2<T>::xUnit = Vector2<T>(1, 0);

template<class T>
const Vector2<T> Matrix2<T>::yUnit = Vector2<T>(0, 1);

typedef Matrix2<float> Matrix2f;
typedef Matrix2<double> Matrix2d;

template<class T>
class Matrix3 : public SpecializedMatrix<3, T, Matrix3<T>, Vector3<T> > {
  typedef SpecializedMatrix<3, T, Matrix3<T>, Vector3<T> > Base;
public:
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
};

typedef Matrix3<float> Matrix3f;
typedef Matrix3<double> Matrix3d;

template<class T>
class Matrix4 : public SpecializedMatrix<4, T, Matrix4<T>, Vector4<T> > {
  typedef SpecializedMatrix<4, T, Matrix4<T>, Vector4<T> > Base;
public:
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
};

typedef Matrix4<float> Matrix4f;
typedef Matrix4<double> Matrix4d;

#endif
