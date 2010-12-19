#include "viewplanes/RowInterlacedViewPlane.h"
#include "viewplanes/ViewPlaneFactory.h"

#include <algorithm>

using namespace std;

class RowInterlaceIterator : public ViewPlane::IteratorBase {
public:
  static const int initialJump = 8;
  
  RowInterlaceIterator(const ViewPlane* plane);
  
  virtual void advance();
  
private:
  int m_rowJump, m_offset;
};

RowInterlaceIterator::RowInterlaceIterator(const ViewPlane* plane)
  : IteratorBase(plane), m_rowJump(RowInterlaceIterator::initialJump), m_offset(0)
{
  
}

void RowInterlaceIterator::advance() {
  m_column++;
  if (m_column == m_plane->width()) {
    m_column = 0;
    m_row += m_rowJump;
    if (m_row >= m_plane->height()) {
      if (m_offset == 1) {
        m_row = m_plane->height();
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

ViewPlane::Iterator RowInterlacedViewPlane::begin() const {
  return Iterator(new RowInterlaceIterator(this));
}

static bool dummy = ViewPlaneFactory::self().registerClass<RowInterlacedViewPlane>("RowInterlacedViewPlane");
