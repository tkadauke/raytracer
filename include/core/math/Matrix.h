#pragma once

#include <iostream>
#include <cmath>
#include "core/math/Vector.h"
#include "core/math/Angle.h"
#include "core/DivisionByZeroException.h"

#include "core/StaticIf.h"

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
  * @tparam Derived the derived class, if any. Defaults to NullType. If this is
  *   NullType, then all the calculation operations, like +, accept as argument
  *   and return an object of type Matrix. If Derived is set explicitely, then
  *   the operators accept and return objects of type Derived. This way,
  *   operators don't have to be redefined in the subclasses. See
  *   Matrix::MatrixType for details.
  */
template<int Dimensions, class T, class VectorType = Vector<Dimensions, T>, class Derived = NullType>
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
  typedef typename StaticIf<
    IsNullType<Derived>::Result,
    ThisType,
    Derived
  >::Result MatrixType;
  
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

  /**
    * Constructs a matrix from cells, a two-dimensional array of size Dim.
    */
  inline Matrix(const CellsType& cells) {
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
  inline Matrix(const Matrix<D, T, V, M>& source) {
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
  inline const T& cell(int row, int col) const {
    return m_cells[row][col];
  }

  /**
    * Sets the cell at coordinate (row, col) to value.
    */
  inline void setCell(int row, int col, const T& value) {
    m_cells[row][col] = value;
  }

  /**
    * @returns the row at index as a constant array.
    */
  inline const RowType& operator[](int index) const {
    return m_cells[index];
  }

  /**
    * @returns the row at index as a mutable array.
    */
  inline RowType& operator[](int index) {
    return m_cells[index];
  }

  /**
    * @returns true, if this matrix equals other, false otherwise.
    */
  inline bool operator==(const MatrixType& other) const {
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
  inline bool operator!=(const MatrixType& other) const {
    return !(*this == other);
  }

  /**
    * @returns the matrix multiplication of this matrix and other.
    */
  inline MatrixType operator*(const MatrixType& other) const {
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
    * @returns the Vector that is the result of the multiplication of this
    *   Matrix and vector.
    */
  inline Vector operator*(const Vector& vector) const {
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
  inline MatrixType operator*(const T& scalar) const {
    MatrixType result;

    for (int row = 0; row != Dimensions; ++row) {
      for (int col = 0; col != Dimensions; ++col) {
        result[row][col] = m_cells[row][col] * scalar;
      }
    }
    return result;
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
  inline MatrixType operator+(const MatrixType& other) const {
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
  inline MatrixType operator/(const T& scalar) const {
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
    * @returns the sum of all elements in row.
    */
  inline T rowSum(int row) const {
    T result = T();
    for (int col = 0; col != Dimensions; ++col) {
      result += m_cells[row][col];
    }
    return result;
  }

  /**
    * @returns the sum of all elements in col.
    */
  inline T colSum(int col) const {
    T result = T();
    for (int row = 0; row != Dimensions; ++row) {
      result += m_cells[row][col];
    }
    return result;
  }
  
  /**
    * @returns the transpose of this Matrix.
    */
  inline MatrixType transposed() const {
    MatrixType result(*this);
    
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
  inline Matrix2()
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
  inline Matrix2(const Matrix<D, T, V, M>& source)
    : Base(source)
  {
  }

  /**
    * Component-wise constructor for a Matrix2.
    */
  inline Matrix2(const T& c00, const T& c01,
                 const T& c10, const T& c11) {
    setCell(0, 0, c00); setCell(0, 1, c01);
    setCell(1, 0, c10); setCell(1, 1, c11);
  }
  
  /**
    * @returns this matrix' determinant.
    */
  inline T determinant() const {
    return cell(0, 0) * cell(1, 1) - cell(0, 1) * cell(1, 0);
  }
  
  /**
    * @returns the inverse \f$M'\f$ of this matrix \f$M\f$, so that \f$M \times
    *   M' = I\f$, where \f$I\f$ is the identity matrix.
    */
  inline Matrix2<T> inverted() const {
    return Matrix2<T>(cell(1, 1), -cell(0, 1), -cell(1, 0), cell(0, 0)) / determinant();
  }
  
  /**
    * @returns a matrix that represents a rotation around the origin with angle.
    */
  template<class A>
  inline static Matrix2<T> rotate(const A& angle) {
    T sin = std::sin(angle.radians()), cos = std::cos(angle.radians());
    return Matrix2<T>(cos, -sin,
                      sin, cos);
  }
  
  /**
    * @returns a matrix that represents a clockwise rotation around the origin
    *   with angle.
    */
  template<class A>
  inline static Matrix2<T> clockwise(const A& angle) {
    return rotate(-angle);
  }
  
  /**
    * @returns a matrix that represents a counter-clockwise rotation around the
    *   origin with angle.
    */
  template<class A>
  inline static Matrix2<T> counterclockwise(const A& angle) {
    return rotate(angle);
  }
  
  /**
    * @returns a matrix that represents a scaling of factor.
    */
  inline static Matrix2<T> scale(const T& factor) {
    return Matrix2<T>(factor, 0,
                      0,      factor);
  }

  /**
    * @returns a matrix that represents a horizontal scaling of xFactor, and
    *   vertical scaling of yFactor.
    */
  inline static Matrix2<T> scale(const T& xFactor, const T& yFactor) {
    return Matrix2<T>(xFactor, 0,
                      0,       yFactor);
  }

  /**
    * @returns a matrix that represents a horizontal sharing of xShear, and
    *   vertical sharing of yShear.
    */
  inline static Matrix2<T> shear(const T& xShear, const T& yShear) {
    return Matrix2<T>(1,      xShear,
                      yShear, 1);
  }

  /**
    * @returns a matrix that represents a reflection along vector.
    */
  inline static Matrix2<T> reflect(const Vector2<T>& vector) {
    return reflect(vector.x(), vector.y());
  }

  /**
    * @returns a matrix that represents a reflection along the line going
    *   through the origin and \f$(x, y)\f$.
    */
  inline static Matrix2<T> reflect(const T& x, const T& y) {
    T len = std::sqrt(x * x + y * y);
    T divider = len*len;
    
    T coordProduct = (2 * x * y) / divider;
    
    return Matrix2<T>((x*x - y*y) / divider, coordProduct,
                      coordProduct,          (y*y - x*x) / divider);
  }

  /**
    * @returns a matrix that represents a reflection along vector.
    */
  inline static Matrix2<T> project(const Vector2<T>& vector) {
    return project(vector.x(), vector.y());
  }

  /**
    * @returns an orthogonal projection matrix, that projects vectors onto the
    *   line going through the origin and \f$(x, y)\f$.
    */
  inline static Matrix2<T> project(const T& x, const T& y) {
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
  using Base::setCell;
  
  /**
    * Default constructor. Constructs the identity matrix
    * 
    * \f[\left(\begin{array}{cccc}
    *   1 & 0 & 0 \\
    *   0 & 1 & 0 \\
    *   0 & 0 & 1 \\
    * \end{array}\right)\f]
    */
  inline Matrix3()
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
  inline Matrix3(const Matrix<D, T, V, M>& source)
    : Base(source)
  {
  }

  /**
    * Component-wise constructor for a Matrix3.
    */
  inline Matrix3(const T& c00, const T& c01, const T& c02,
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
  inline static Matrix3<T> rotateX(const A& angle) {
    T sin = std::sin(angle.radians()), cos = std::cos(angle.radians());
    return Matrix3<T>(T(1), T(), T(),
                      T(),  cos, -sin,
                      T(),  sin, cos);
  }
  
  /**
    * @returns a matrix that represents a rotation around the y axis with angle.
    */
  template<class A>
  inline static Matrix3<T> rotateY(const A& angle) {
    T sin = std::sin(angle.radians()), cos = std::cos(angle.radians());
    return Matrix3<T>(cos,  T(),  sin,
                      T(),  T(1), T(),
                      -sin, T(),  cos);
  }
  
  /**
    * @returns a matrix that represents a rotation around the z axis with angle.
    */
  template<class A>
  inline static Matrix3<T> rotateZ(const A& angle) {
    T sin = std::sin(angle.radians()), cos = std::cos(angle.radians());
    return Matrix3<T>(cos, -sin, T(),
                      sin, cos,  T(),
                      T(), T(),  T(1));
  }
  
  /**
    * @returns a matrix that represents a rotation around all axes with angles
    *   given in the angles Vector3.
    */
  inline static Matrix3<T> rotate(const Vector3<T>& angles) {
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
  inline static Matrix3<T> rotate(const A& x, const A& y, const A& z) {
    return rotateX(x) * rotateY(y) * rotateZ(z);
  }
  
  /**
    * @returns a matrix that represents a scaling of factor.
    */
  inline static Matrix3<T> scale(const T& factor) {
    return Matrix3<T>(factor, T(),    T(),
                      T(),    factor, T(),
                      T(),    T(),    factor);
  }

  /**
    * @returns a matrix that represents a scaling of the x axis with xFactor, 
    *   the y axis with yFactor, and the z axis with zFactor.
    */
  inline static Matrix3<T> scale(const T& xFactor, const T& yFactor, const T& zFactor) {
    return Matrix3<T>(xFactor, T(),     T(),
                      T(),     yFactor, T(),
                      T(),     T(),     zFactor);
  }

  /**
    * @returns a matrix that represents a scaling of three axes given by the
    *   components of the factor Vector3.
    */
  inline static Matrix3<T> scale(const Vector3<T>& factor) {
    return Matrix3<T>(factor[0], T(),       T(),
                      T(),       factor[1], T(),
                      T(),       T(),       factor[2]);
  }
};

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
  inline Matrix4()
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
  inline Matrix4(const Matrix<D, T, V, M>& source)
    : Base(source)
  {
  }

  /**
    * Component-wise constructor for a Matrix4.
    */
  inline Matrix4(const T& c00, const T& c01, const T& c02, const T& c03,
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
  inline Matrix4(const Vector3<T>& xAxis, const Vector3<T>& yAxis, const Vector3<T>& zAxis) {
    setCell(0, 0, xAxis[0]); setCell(0, 1, xAxis[1]); setCell(0, 2, xAxis[2]); setCell(0, 3, T());
    setCell(1, 0, yAxis[0]); setCell(1, 1, yAxis[1]); setCell(1, 2, yAxis[2]); setCell(1, 3, T());
    setCell(2, 0, zAxis[0]); setCell(2, 1, zAxis[1]); setCell(2, 2, zAxis[2]); setCell(2, 3, T());
    setCell(3, 0, T());      setCell(3, 1, T());      setCell(3, 2, T());      setCell(3, 3, T(1));
  }
  
  /**
    * @returns the inverse \f$M'\f$ of this matrix \f$M\f$, so that \f$M \times
    *   M' = I\f$, where \f$I\f$ is the identity matrix.
    */
  Matrix4<T> inverted() const;
  T determinant() const;
  
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
  inline static Matrix4<T> translate(const Vector3<T>& position) {
    return Matrix4<T>(
      1, 0, 0, position[0],
      0, 1, 0, position[1],
      0, 0, 1, position[2],
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
