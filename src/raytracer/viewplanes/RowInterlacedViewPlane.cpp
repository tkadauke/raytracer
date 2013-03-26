#include "raytracer/viewplanes/RowInterlacedViewPlane.h"
#include "raytracer/viewplanes/ViewPlaneFactory.h"

#include <algorithm>
#include <cmath>

using namespace std;
using namespace raytracer;

namespace {
  class RowInterlaceIterator : public ViewPlane::IteratorBase {
  public:
    RowInterlaceIterator(const ViewPlane* plane, const Rect& rect);

    virtual void advance();

  private:
    int initialJump() const;
  
    int m_rowJump, m_offset;
  };

  RowInterlaceIterator::RowInterlaceIterator(const ViewPlane* plane, const Rect& rect)
    : IteratorBase(plane, rect), m_rowJump(initialJump()), m_offset(0)
  {
  }

  void RowInterlaceIterator::advance() {
    m_column++;
    if (m_column == m_rect.width()) {
      m_column = 0;
      m_row += m_rowJump;
      if (m_row >= m_rect.height()) {
        if (m_offset == 1) {
          m_row = m_rect.height();
        } else {
          if (m_offset == 0) {
            m_offset = m_rowJump / 2;
          } else {
            m_offset /= 2;
            m_rowJump /= 2;
          }
          m_row = m_offset;
        }
      }
    }
  }

  int RowInterlaceIterator::initialJump() const {
    return 1 << int(log(m_rect.height()));
  }
}

ViewPlane::Iterator RowInterlacedViewPlane::begin(const Rect& rect) const {
  return Iterator(new RowInterlaceIterator(this, rect));
}

static bool dummy = ViewPlaneFactory::self().registerClass<RowInterlacedViewPlane>("RowInterlacedViewPlane");
