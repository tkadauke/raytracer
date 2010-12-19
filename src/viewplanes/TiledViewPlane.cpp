#include "viewplanes/TiledViewPlane.h"
#include "viewplanes/ViewPlaneFactory.h"

class TileIterator : public ViewPlane::IteratorBase {
public:
  TileIterator(const ViewPlane* plane, const Rect& rect);
  
  virtual void advance();

private:
  int m_tileSize;
  int m_xTile, m_yTile;
};

TileIterator::TileIterator(const ViewPlane* plane, const Rect& rect)
  : IteratorBase(plane, rect), m_tileSize(32), m_xTile(0), m_yTile(0)
{
}

void TileIterator::advance() {
  m_column++;
  if (m_column == m_rect.width() || m_column == m_xTile + m_tileSize) {
    m_column = m_xTile;
    m_row++;
  }
  if (m_row == m_rect.height() || m_row == m_yTile + m_tileSize) {
    m_xTile += m_tileSize;
    if (m_xTile >= m_rect.width()) {
      m_xTile = 0;
      m_yTile += m_tileSize;
    }
    m_column = m_xTile;
    m_row = m_yTile;
  }
  if (m_row >= m_rect.height()) {
    m_row = m_rect.height();
    m_column = 0;
  }
}

ViewPlane::Iterator TiledViewPlane::begin(const Rect& rect) const {
  return Iterator(new TileIterator(this, rect));
}

static bool dummy = ViewPlaneFactory::self().registerClass<TiledViewPlane>("TiledViewPlane");
