#include "raytracer/cameras/Camera.h"
#include "core/math/Rect.h"
#include "raytracer/viewplanes/ViewPlane.h"
#include "raytracer/viewplanes/PointInterlacedViewPlane.h"
#include "core/Buffer.h"

using namespace raytracer;

Camera::~Camera() {
  if (m_viewPlane)
    delete m_viewPlane;
}

ViewPlane* Camera::viewPlane() {
  // Use this type of view plane as default, because I like it the most :-)
  if (!m_viewPlane)
    m_viewPlane = new PointInterlacedViewPlane;
  return m_viewPlane;
}

void Camera::setViewPlane(ViewPlane* plane) {
  if (m_viewPlane)
    delete m_viewPlane;
  m_viewPlane = plane;
}

const Matrix4d& Camera::matrix() {
  if (!m_matrix) {
    auto zAxis = (m_target - m_position).normalized();
    auto xAxis = Vector3d::up() ^ zAxis;
    auto yAxis = xAxis ^ -zAxis;
    
    m_matrix = Matrix4d(xAxis, yAxis, zAxis).inverted();
    m_matrix.value().setCell(0, 3, m_position[0]);
    m_matrix.value().setCell(1, 3, m_position[1]);
    m_matrix.value().setCell(2, 3, m_position[2]);
  }
  return m_matrix;
}

void Camera::render(Raytracer* raytracer, Buffer<unsigned int>& buffer) {
  render(raytracer, buffer, Rect(0, 0, buffer.width(), buffer.height()));
}

void Camera::plot(Buffer<unsigned int>& buffer, const Rect& rect, const ViewPlane::Iterator& pixel, const Colord& color) {
  int size = pixel.pixelSize();
  if (size == 1) {
    buffer[pixel.row()][pixel.column()] = color.rgb();
  } else {
    unsigned int rgb = color.rgb();
    for (int x = pixel.column(); x != pixel.column() + size && x < rect.right(); ++x)
      for (int y = pixel.row(); y != pixel.row() + size && y < rect.bottom(); ++y)
        buffer[y][x] = rgb;
  }
}
