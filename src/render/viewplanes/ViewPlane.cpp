#include "render/viewplanes/ViewPlane.h"
#include "render/viewplanes/ViewPlaneFactory.h"

#include "render/samplers/RegularSampler.h"

using namespace render;

ViewPlane::ViewPlane()
    : m_pixelSize(1),
      m_aspectMode(AspectMode::Stretch),
      m_aspectRatio(0.0),
      m_hSpan(8.0),
      m_vSpan(6.0),
      m_sampler(std::make_shared<render::RegularSampler>()) {
  m_sampler->setup(1, 1);
}

ViewPlane::ViewPlane(const Matrix4d& matrix, const Recti& window)
    : ViewPlane() {
  setup(matrix, window);
}

ViewPlane::~ViewPlane() {
}

std::shared_ptr<ViewPlane> ViewPlane::clone() const {
  return std::make_shared<ViewPlane>(*this);
}

void ViewPlane::setupVectors() {
  if (m_aspectMode == AspectMode::FitExact) {
    double intrinsicAspect = m_aspectRatio > 0.0 ? m_aspectRatio : (4.0 / 3.0);
    double bufferAspect = static_cast<double>(width()) / height();

    int innerW, innerH, offsetX = 0, offsetY = 0;
    if (bufferAspect > intrinsicAspect) {
      // Buffer is wider than intrinsic → pillarbox bars on left/right.
      innerH = height();
      innerW = static_cast<int>(height() * intrinsicAspect);
      offsetX = (width() - innerW) / 2;
    } else {
      // Buffer is taller than intrinsic → letterbox bars on top/bottom.
      innerW = width();
      innerH = static_cast<int>(width() / intrinsicAspect);
      offsetY = (height() - innerH) / 2;
    }
    m_innerRect = Recti(m_window.left() + offsetX, m_window.top() + offsetY, innerW, innerH);

    m_hSpan = 8.0;
    m_vSpan = m_hSpan / intrinsicAspect;
    Vector3d r = Matrix3d(m_matrix) * (Vector3d(1, 0, 0) / innerW * m_hSpan);
    Vector3d d = Matrix3d(m_matrix) * (Vector3d(0, 1, 0) / innerH * m_vSpan);
    // Offset m_topLeft so that pixelAt(innerRect.left, innerRect.top)
    // lands at the inner rect's top-left in camera space.
    Vector3d tlCam = m_matrix * Vector4d(-m_hSpan / 2.0, -m_vSpan / 2.0, 0);
    m_right = r;
    m_down = d;
    m_topLeft = tlCam - r * m_innerRect.left() - d * m_innerRect.top();
    setupScaledVectors();
    return;
  }

  switch (m_aspectMode) {
  case AspectMode::FitWidth:
    // H-FOV constant at 8 world units; V derived from buffer shape.
    m_hSpan = 8.0;
    m_vSpan = 8.0 * height() / width();
    break;
  case AspectMode::FitHeight:
    // V-FOV constant at 6 world units; H derived from buffer shape.
    m_vSpan = 6.0;
    m_hSpan = 6.0 * width() / height();
    break;
  default: // Stretch: fixed 8×6, matches pre-AspectMode behavior.
    m_hSpan = 8.0;
    m_vSpan = 6.0;
    break;
  }

  m_right = Matrix3d(m_matrix) * (Vector3d(1, 0, 0) / width() * m_hSpan);
  m_down = Matrix3d(m_matrix) * (Vector3d(0, 1, 0) / height() * m_vSpan);
  m_topLeft = m_matrix * Vector4d(-m_hSpan / 2.0, -m_vSpan / 2.0, 0) - m_right * m_window.left() -
              m_down * m_window.top();
  m_innerRect = m_window;
  setupScaledVectors();
}

void ViewPlane::setupScaledVectors() {
  const Vector3d cameraPos = m_matrix.translationVector();
  m_scaledTopLeft = cameraPos + (m_topLeft - cameraPos) * m_pixelSize;
  m_scaledRight = m_right * m_pixelSize;
  m_scaledDown = m_down * m_pixelSize;
}

ViewPlane::Iterator ViewPlane::begin(const Recti& rect) const {
  return Iterator(new RegularIterator(this, rect));
}

Recti ViewPlane::Iterator::footprintWithin(const Recti& bounds) const {
  const int left = std::max(column(), bounds.left());
  const int top = std::max(row(), bounds.top());
  const int right = std::min(column() + pixelSize(), bounds.right());
  const int bottom = std::min(row() + pixelSize(), bounds.bottom());
  if (left >= right || top >= bottom) {
    return Recti(left, top, 0, 0);
  }
  return Recti(left, top, right - left, bottom - top);
}

ViewPlane::IteratorBase::IteratorBase(const ViewPlane* plane, const Recti& rect)
    : m_plane(plane),
      m_rect(rect),
      m_column(0),
      m_row(0),
      m_pixelSize(1) {
}

ViewPlane::IteratorBase::IteratorBase(const ViewPlane* plane, const Recti& rect, bool)
    : m_plane(plane),
      m_rect(rect),
      m_column(0),
      m_row(rect.height()),
      m_pixelSize(1) {
}

Vector3d ViewPlane::IteratorBase::current() const {
  return m_plane->pixelAt(column(), row());
}

ViewPlane::RegularIterator::RegularIterator(const ViewPlane* plane, const Recti& rect)
    : IteratorBase(plane, rect) {
}

ViewPlane::RegularIterator::RegularIterator(const ViewPlane* plane, const Recti& rect, bool end)
    : IteratorBase(plane, rect, end) {
}

void ViewPlane::RegularIterator::advance() {
  m_column++;
  if (m_column == m_rect.width()) {
    m_column = 0;
    m_row++;
  }
}

static bool dummy = ViewPlaneFactory::self().registerClass<ViewPlane>("ViewPlane");
