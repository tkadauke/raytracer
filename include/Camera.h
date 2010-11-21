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
      
      m_matrix = Matrix4d(xAxis, yAxis, zAxis).inverted() * Matrix4d::translate(m_position);
    }
    return m_matrix;
  }

private:
  Vector3d m_position, m_target;
  MemoizedValue<Matrix4d> m_matrix;
};

#endif
