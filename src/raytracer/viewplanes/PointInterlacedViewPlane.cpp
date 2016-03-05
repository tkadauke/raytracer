#include "raytracer/viewplanes/PointInterlacedViewPlane.h"
#include "raytracer/viewplanes/ViewPlaneFactory.h"

#include <algorithm>
#include <cmath>

using namespace std;
using namespace raytracer;

namespace {
  class PointInterlaceIterator : public ViewPlane::IteratorBase {
  public:
    PointInterlaceIterator(const ViewPlane* plane, const Recti& rect);

    virtual void advance();

  private:
    int initialSize() const;

    bool m_evenRow;
    int m_initialSize;
  };

  PointInterlaceIterator::PointInterlaceIterator(const ViewPlane* plane, const Recti& rect)
    : IteratorBase(plane, rect),
      m_evenRow(false),
      m_initialSize(initialSize())
  {
    m_pixelSize = m_initialSize;
  }

  void PointInterlaceIterator::advance() {
    m_column += m_evenRow ? m_pixelSize * 2 : m_pixelSize;
    if (m_column >= m_rect.width()) {
      // next row
      m_row += m_pixelSize;
      if (m_row >= m_rect.height()) {
        if (m_pixelSize == 1) {
          // end
          m_row = m_rect.height();
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
        if (m_pixelSize != m_initialSize)
          m_evenRow = !m_evenRow;
        m_column = m_evenRow ? m_pixelSize : 0;
      }
    }
  }

  int PointInterlaceIterator::initialSize() const {
    return min(1 << int(log(m_rect.width())), 1 << int(log(m_rect.height())));
  }
}

ViewPlane::Iterator PointInterlacedViewPlane::begin(const Recti& rect) const {
  return Iterator(new PointInterlaceIterator(this, rect));
}

static bool dummy = ViewPlaneFactory::self().registerClass<PointInterlacedViewPlane>("PointInterlacedViewPlane");
