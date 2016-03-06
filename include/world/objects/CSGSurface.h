#pragma once

#include "world/objects/Surface.h"

class CSGSurface : public Surface {
  Q_OBJECT;
  Q_PROPERTY(bool active READ active WRITE setActive);
  
public:
  CSGSurface(Element* parent = nullptr);
  
  bool active() const { return m_active; }
  void setActive(bool active) { m_active = active; }
  
private:
  bool m_active;
};
