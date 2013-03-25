#ifndef COMPOSITE_H
#define COMPOSITE_H

#include <list>

#include "raytracer/primitives/Primitive.h"

class Composite : public Primitive {
public:
  typedef std::list<Primitive*> Primitives;
  
  inline Composite() {}
  ~Composite();
  
  virtual Primitive* intersect(const Ray& ray, HitPointInterval& hitPoints);
  virtual bool intersects(const Ray& ray);
  virtual BoundingBox boundingBox();

  inline void add(Primitive* primitive) { primitive->setParent(this); m_primitives.push_back(primitive); }
  
  inline const Primitives& primitives() const { return m_primitives; }
  
private:
  Primitives m_primitives;
};

#endif
