#include "RowShuffledViewPlane.h"

#include <algorithm>

using namespace std;

class RowShuffleIterator : public ViewPlane::IteratorBase {
public:
  RowShuffleIterator(const ViewPlane* plane);
  
  virtual void advance();
  
private:
  std::vector<int> m_rowIndices;
  int m_rowIndex;
};

RowShuffleIterator::RowShuffleIterator(const ViewPlane* plane)
  : IteratorBase(plane), m_rowIndex(0)
{
  for (int i = 0; i != plane->height(); ++i)
    m_rowIndices.push_back(i);
  random_shuffle(m_rowIndices.begin(), m_rowIndices.end());
  m_row = m_rowIndices[0];
}

void RowShuffleIterator::advance() {
  m_column++;
  if (m_column == m_plane->width()) {
    m_column = 0;
    m_rowIndex++;
    if (m_rowIndex == m_plane->height())
      m_row = m_plane->height();
    else
      m_row = m_rowIndices[m_rowIndex];
  }
}

ViewPlane::Iterator RowShuffledViewPlane::begin() const {
  return Iterator(new RowShuffleIterator(this));
}
