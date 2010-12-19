#include "cameras/Camera.h"
#include "viewplanes/ViewPlane.h"
#include "viewplanes/PointShuffledViewPlane.h"
#include "Buffer.h"

Camera::~Camera() {
  if (m_viewPlane)
    delete m_viewPlane;
}

ViewPlane* Camera::viewPlane() {
  // Use this type of view plane as default, because I like it the most :-)
  if (!m_viewPlane)
    m_viewPlane = new PointShuffledViewPlane;
  return m_viewPlane;
}

void Camera::setViewPlane(ViewPlane* plane) {
  if (m_viewPlane)
    delete m_viewPlane;
  m_viewPlane = plane;
}

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

void Camera::plot(Buffer& buffer, const ViewPlane::Iterator& pixel, const Colord& color) {
  int size = pixel.pixelSize();
  if (size == 1) {
    buffer[pixel.row()][pixel.column()] = color;
  } else {
    for (int x = pixel.column(); x != pixel.column() + size && x < buffer.width(); ++x)
      for (int y = pixel.row(); y != pixel.row() + size && y < buffer.height(); ++y)
        buffer[y][x] = color;
  }
}
