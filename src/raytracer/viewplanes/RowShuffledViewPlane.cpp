#include "raytracer/viewplanes/RowShuffledViewPlane.h"
#include "raytracer/viewplanes/ViewPlaneFactory.h"

#include <algorithm>

using namespace std;
using namespace raytracer;

namespace {
  class RowShuffleIterator : public ViewPlane::IteratorBase {
  public:
    RowShuffleIterator(const ViewPlane* plane, const Recti& rect);

    virtual void advance();

  private:
    std::vector<int> m_rowIndices;
    int m_rowIndex;
  };

  RowShuffleIterator::RowShuffleIterator(const ViewPlane* plane, const Recti& rect)
    : IteratorBase(plane, rect),
      m_rowIndex(0)
  {
    for (int i = 0; i != rect.height(); ++i)
      m_rowIndices.push_back(i);
    random_shuffle(m_rowIndices.begin(), m_rowIndices.end());
    m_row = m_rowIndices[0];
  }

  void RowShuffleIterator::advance() {
    m_column++;
    if (m_column == m_rect.width()) {
      m_column = 0;
      m_rowIndex++;
      if (m_rowIndex == m_rect.height())
        m_row = m_rect.height();
      else
        m_row = m_rowIndices[m_rowIndex];
    }
  }
}

ViewPlane::Iterator RowShuffledViewPlane::begin(const Recti& rect) const {
  return Iterator(new RowShuffleIterator(this, rect));
}

static bool dummy = ViewPlaneFactory::self().registerClass<RowShuffledViewPlane>("RowShuffledViewPlane");
