#ifndef CAMERA_H
#define CAMERA_H

#include "Vector.h"
#include "Matrix.h"
#include "MemoizedValue.h"

class Camera {
public:
  Camera(const Vector3d& position, const Vector3d& target)
    : m_position(position), m_target(target) {}
  
  const Matrix4d& matrix() {
    if (!m_matrix) {
      Vector3d zAxis = (m_target - m_position).normalized();
      Vector3d up(0, 1, 0);
      Vector3d xAxis = up ^ zAxis;
      Vector3d yAxis = xAxis ^ -zAxis;
      
      m_matrix = Matrix4d(xAxis, yAxis, zAxis).inverted();
      m_matrix.value().setCell(0, 3, m_position[0]);
      m_matrix.value().setCell(1, 3, m_position[1]);
      m_matrix.value().setCell(2, 3, m_position[2]);
    }
    return m_matrix;
  }

private:
  Vector3d m_position, m_target;
  MemoizedValue<Matrix4d> m_matrix;
};

#endif
