#ifndef SURFACE_H
#define SURFACE_H

class Ray;
class HitPointInterval;
class Composite;
class Material;

class Surface {
public:
  Surface() {}
  virtual ~Surface() {}
  
  virtual Surface* intersect(const Ray& ray, HitPointInterval& hitPoints) = 0;
  virtual bool intersects(const Ray& ray);
  
  inline void setMaterial(Material* material) { m_material = material; }
  inline Material* material() const { return m_material; }
  
  inline void setParent(Composite* parent) { m_parent = parent; }
  inline Composite* parent() const { return m_parent; }

protected:
  Composite* m_parent;

private:
  Material* m_material;
};

#endif
