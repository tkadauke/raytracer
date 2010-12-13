#include "PointInterlacedViewPlane.h"

#include <algorithm>

using namespace std;

class PointInterlaceIterator : public ViewPlane::IteratorBase {
public:
  static const int initialSize = 8;
  
  PointInterlaceIterator(const ViewPlane* plane);
  
  virtual void advance();
  
private:
  bool m_evenRow;
};

PointInterlaceIterator::PointInterlaceIterator(const ViewPlane* plane)
  : IteratorBase(plane), m_evenRow(false)
{
  m_pixelSize = PointInterlaceIterator::initialSize;
}

void PointInterlaceIterator::advance() {
  m_column += m_evenRow ? m_pixelSize * 2 : m_pixelSize;
  if (m_column >= m_plane->width()) {
    // next row
    m_row += m_pixelSize;
    if (m_row >= m_plane->height()) {
      if (m_pixelSize == 1) {
        // end
        m_row = m_plane->height();
        m_column = 0;
      } else {
        // next iteration
        m_evenRow = true;
        m_pixelSize /= 2;
        m_row = 0;
        m_column = m_pixelSize;
      }
    } else {
      // in the first iteration, there is nothing rendered yet, so we never skip any pixels,
      // i.e. all rows are "odd". In subsequent iterations, flip the switch
      if (m_pixelSize != PointInterlaceIterator::initialSize)
        m_evenRow = !m_evenRow;
      m_column = m_evenRow ? m_pixelSize : 0;
    }
  }
}

ViewPlane::Iterator PointInterlacedViewPlane::begin() const {
  return Iterator(new PointInterlaceIterator(this));
}
