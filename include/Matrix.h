#ifndef MATRIX_H
#define MATRIX_H

#include <iostream>
#include "Vector.h"

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

template<class T>
class Matrix2 : public Matrix<2, T> {
  typedef Matrix<2, T> Base;
public:
  inline Matrix2()
    : Base()
  {
  }

  inline Matrix2(const T& c00, const T& c01,
                 const T& c10, const T& c11) {
    setCell(0, 0, c00); setCell(0, 1, c01);
    setCell(1, 0, c10); setCell(1, 1, c11);
  }
};

template<class T>
class Matrix3 : public Matrix<3, T> {
  typedef Matrix<3, T> Base;
public:
  inline Matrix3()
    : Base()
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

template<class T>
class Matrix4 : public Matrix<4, T> {
  typedef Matrix<4, T> Base;
public:
  inline Matrix4()
    : Base()
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

#endif
