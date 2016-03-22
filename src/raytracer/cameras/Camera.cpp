#include "raytracer/cameras/Camera.h"
#include "core/math/Rect.h"
#include "raytracer/viewplanes/ViewPlane.h"
#include "raytracer/viewplanes/PointInterlacedViewPlane.h"
#include "raytracer/samplers/Sampler.h"
#include "core/Buffer.h"
#include "raytracer/Raytracer.h"
#include "raytracer/State.h"

using namespace raytracer;

Camera::Camera()
  : m_cancelled(false),
    m_viewPlane(std::make_shared<PointInterlacedViewPlane>())
{
}

Camera::Camera(const Vector3d& position, const Vector3d& target)
  : Camera()
{
  m_position = position;
  m_target = target;
}

Camera::~Camera() {
}

void Camera::setViewPlane(std::shared_ptr<ViewPlane> plane) {
  m_viewPlane = plane;
}

const Matrix4d& Camera::matrix() const {
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

void Camera::render(std::shared_ptr<Raytracer> raytracer, Buffer<unsigned int>& buffer) const {
  render(raytracer, buffer, Recti(0, 0, buffer.width(), buffer.height()));
}

void Camera::render(std::shared_ptr<Raytracer> raytracer, Buffer<unsigned int>& buffer, const Recti& rect) const {
  if (isCancelled())
    return;
  
  auto plane = viewPlane();

  for (ViewPlane::Iterator pixel = plane->begin(rect), end = plane->end(rect); pixel != end; ++pixel) {
    Colord pixelColor;
    for (const auto& sample : plane->sampler()->sampleSet()) {
      Rayd ray = rayForPixel(pixel.pixel() + sample);
      if (ray.direction().isDefined()) {
        State state;
        pixelColor += raytracer->rayColor(ray, state);
      }
    }
    
    plot(buffer, rect, pixel, pixelColor);
    
    if (isCancelled())
      break;
  }
}

void Camera::plot(Buffer<unsigned int>& buffer, const Recti& rect, const ViewPlane::Iterator& pixel, const Colord& color) const {
  auto avergageColor = color / viewPlane()->sampler()->numSamples();
  unsigned int rgb = avergageColor.rgb();
  
  int size = pixel.pixelSize();
  if (size == 1) {
    buffer[pixel.row()][pixel.column()] = rgb;
  } else {
    for (int x = pixel.column(); x != pixel.column() + size && x < rect.right(); ++x)
      for (int y = pixel.row(); y != pixel.row() + size && y < rect.bottom(); ++y)
        buffer[y][x] = rgb;
  }
}
