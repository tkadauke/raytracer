#include "surfaces/Composite.h"

Composite::~Composite() {
  for (Surfaces::const_iterator i = m_surfaces.begin(); i != m_surfaces.end(); ++i)
    delete *i;
}

BoundingBox Composite::boundingBox() {
  BoundingBox b;
  for (Surfaces::const_iterator i = m_surfaces.begin(); i != m_surfaces.end(); ++i)
    b.include((*i)->boundingBox());
  return b;
}
