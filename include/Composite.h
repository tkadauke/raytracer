#ifndef COMPOSITE_H
#define COMPOSITE_H

#include <list>

#include "Surface.h"
#include "Color.h"

class Composite : public Surface {
public:
  typedef std::list<Surface*> Surfaces;
  
  inline Composite() {}
  ~Composite();
  
  inline void add(Surface* surface) { m_surfaces.push_back(surface); }
  
  inline const Surfaces& surfaces() const { return m_surfaces; }
  
private:
  Surfaces m_surfaces;
};

#endif
