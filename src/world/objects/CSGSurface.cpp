#include "world/objects/CSGSurface.h"

CSGSurface::CSGSurface(Element* parent)
  : Surface(parent), m_active(true)
{
}

#include "CSGSurface.moc"
