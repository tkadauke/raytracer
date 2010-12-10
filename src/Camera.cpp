#include "Camera.h"

const Matrix4d& Camera::matrix() {
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
