#include "ViewPlane.h"
#include <iostream>
#include <algorithm>

using namespace std;

void ViewPlane::setupVectors() {
  m_topLeft = m_matrix * Vector3d(-4, -3, 0);
  m_right = Matrix3d(m_matrix) * (Vector3d(1, 0, 0) / m_width * 8.0);
  m_down = Matrix3d(m_matrix) * (Vector3d(0, 1, 0) / m_height * 6.0);
}

ViewPlane::Iterator ViewPlane::begin() const {
  return Iterator(new RegularIterator(this));
}


ViewPlane::IteratorBase::IteratorBase(const ViewPlane* plane)
  : m_plane(plane), m_column(0), m_row(0)
{
}

ViewPlane::IteratorBase::IteratorBase(const ViewPlane* plane, bool)
  : m_plane(plane), m_column(0), m_row(plane->m_height)
{
}

Vector3d ViewPlane::IteratorBase::current() const {
  return m_plane->m_topLeft + m_plane->m_right * m_column + m_plane->m_down * m_row;
}


ViewPlane::RegularIterator::RegularIterator(const ViewPlane* plane)
  : IteratorBase(plane)
{
}

ViewPlane::RegularIterator::RegularIterator(const ViewPlane* plane, bool end)
  : IteratorBase(plane, end)
{
}

void ViewPlane::RegularIterator::advance() {
  m_column++;
  if (m_column == m_plane->m_width) {
    m_column = 0;
    m_row++;
  }
}
