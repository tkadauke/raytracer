#include "TiledViewPlane.h"

class TileIterator : public ViewPlane::IteratorBase {
public:
  TileIterator(const ViewPlane* plane);
  
  virtual void advance();

private:
  int m_tileSize;
  int m_xTile, m_yTile;
};

TileIterator::TileIterator(const ViewPlane* plane)
  : IteratorBase(plane), m_tileSize(32), m_xTile(0), m_yTile(0)
{
}

void TileIterator::advance() {
  m_column++;
  if (m_column == m_plane->width() || m_column == m_xTile + m_tileSize) {
    m_column = m_xTile;
    m_row++;
  }
  if (m_row == m_plane->height() || m_row == m_yTile + m_tileSize) {
    m_xTile += m_tileSize;
    if (m_xTile >= m_plane->width()) {
      m_xTile = 0;
      m_yTile += m_tileSize;
    }
    m_column = m_xTile;
    m_row = m_yTile;
  }
  if (m_row >= m_plane->height()) {
    m_row = m_plane->height();
    m_column = 0;
  }
}

ViewPlane::Iterator TiledViewPlane::begin() const {
  return Iterator(new TileIterator(this));
}
