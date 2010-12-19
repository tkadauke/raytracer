#include "viewplanes/PointShuffledViewPlane.h"
#include "viewplanes/ViewPlaneFactory.h"

#include <vector>
#include <algorithm>

using namespace std;

class PointShuffleIterator : public ViewPlane::IteratorBase {
public:
  PointShuffleIterator(const ViewPlane* plane);
  
  virtual void advance();
  
private:
  inline void setRowAndColumnFromIndex() {
    m_row = m_pointIndices[m_pointIndex].first;
    m_column = m_pointIndices[m_pointIndex].second;
  }
  
  vector<pair<int, int> > m_pointIndices;
  unsigned int m_pointIndex;
};

PointShuffleIterator::PointShuffleIterator(const ViewPlane* plane)
  : IteratorBase(plane), m_pointIndex(0)
{
  for (int i = 0; i != plane->height(); ++i) {
    for (int j = 0; j != plane->width(); ++j)
      m_pointIndices.push_back(make_pair(i, j));
  }
  random_shuffle(m_pointIndices.begin(), m_pointIndices.end());
  setRowAndColumnFromIndex();
}

void PointShuffleIterator::advance() {
  m_pointIndex++;
  if (m_pointIndex == m_pointIndices.size()) {
    m_row = m_plane->height();
    m_column = 0;
  } else {
    setRowAndColumnFromIndex();
  }
}

ViewPlane::Iterator PointShuffledViewPlane::begin() const {
  return Iterator(new PointShuffleIterator(this));
}

static bool dummy = ViewPlaneFactory::self().registerClass<PointShuffledViewPlane>("PointShuffledViewPlane");
