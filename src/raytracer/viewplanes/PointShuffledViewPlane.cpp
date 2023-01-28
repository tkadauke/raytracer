#include "raytracer/viewplanes/PointShuffledViewPlane.h"
#include "raytracer/viewplanes/ViewPlaneFactory.h"
#include "core/util/Random.h"

#include <vector>

using namespace std;
using namespace raytracer;

namespace {
  class PointShuffleIterator : public ViewPlane::IteratorBase {
  public:
    PointShuffleIterator(const ViewPlane* plane, const Recti& rect);

    virtual void advance();

  private:
    inline void setRowAndColumnFromIndex() {
      m_row = m_pointIndices[m_pointIndex].first;
      m_column = m_pointIndices[m_pointIndex].second;
    }

    vector<pair<int, int>> m_pointIndices;
    unsigned int m_pointIndex;
  };

  PointShuffleIterator::PointShuffleIterator(const ViewPlane* plane, const Recti& rect)
    : IteratorBase(plane, rect),
      m_pointIndex(0)
  {
    for (int i = 0; i != rect.height(); ++i) {
      for (int j = 0; j != rect.width(); ++j)
        m_pointIndices.push_back(make_pair(i, j));
    }
    random_shuffle(m_pointIndices.begin(), m_pointIndices.end());
    setRowAndColumnFromIndex();
  }

  void PointShuffleIterator::advance() {
    m_pointIndex++;
    if (m_pointIndex == m_pointIndices.size()) {
      m_row = m_rect.height();
      m_column = 0;
    } else {
      setRowAndColumnFromIndex();
    }
  }
}

ViewPlane::Iterator PointShuffledViewPlane::begin(const Recti& rect) const {
  return Iterator(new PointShuffleIterator(this, rect));
}

static bool dummy = ViewPlaneFactory::self().registerClass<PointShuffledViewPlane>("PointShuffledViewPlane");
