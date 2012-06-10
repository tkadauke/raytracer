#include "viewplanes/ViewPlane.h"
#include "viewplanes/ViewPlaneFactory.h"

void ViewPlane::setupVectors() {
  m_topLeft = m_matrix * Vector4d(-4, -3, 0);
  m_right = Matrix3d(m_matrix) * (Vector3d(1, 0, 0) / width() * 8.0);
  m_down = Matrix3d(m_matrix) * (Vector3d(0, 1, 0) / height() * 6.0);
}

ViewPlane::Iterator ViewPlane::begin(const Rect& rect) const {
  return Iterator(new RegularIterator(this, rect));
}


ViewPlane::IteratorBase::IteratorBase(const ViewPlane* plane, const Rect& rect)
  : m_plane(plane), m_rect(rect), m_column(0), m_row(0), m_pixelSize(1)
{
}

ViewPlane::IteratorBase::IteratorBase(const ViewPlane* plane, const Rect& rect, bool)
  : m_plane(plane), m_rect(rect), m_column(0), m_row(rect.height())
{
}

Vector3d ViewPlane::IteratorBase::current() const {
  return (m_plane->m_topLeft + m_plane->m_right * column() + m_plane->m_down * row()) * m_plane->pixelSize();
}


ViewPlane::RegularIterator::RegularIterator(const ViewPlane* plane, const Rect& rect)
  : IteratorBase(plane, rect)
{
}

ViewPlane::RegularIterator::RegularIterator(const ViewPlane* plane, const Rect& rect, bool end)
  : IteratorBase(plane, rect, end)
{
}

void ViewPlane::RegularIterator::advance() {
  m_column++;
  if (m_column == m_rect.width()) {
    m_column = 0;
    m_row++;
  }
}

static bool dummy = ViewPlaneFactory::self().registerClass<ViewPlane>("ViewPlane");
