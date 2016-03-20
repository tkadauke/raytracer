#pragma once

#include "world/objects/PhongMaterial.h"

/**
  * Transparent materials are used to describe perfectly transparent materials
  * like air, water, glass, or diamonds, that may be tinted on the outside,
  * but don't filter the light while it travels through them. The [refraction
  * index](https://en.wikipedia.org/wiki/Refractive_index) describes how light
  * propagates through the medium.
  * 
  * @image html transparent_material.png "Transparent material"
  */
class TransparentMaterial : public PhongMaterial {
  Q_OBJECT;
  Q_PROPERTY(double refractionIndex READ refractionIndex WRITE setRefractionIndex);

public:
  explicit TransparentMaterial(Element* parent = nullptr);

  /**
    * @returns the material's index of refraction
    */
  inline double refractionIndex() const {
    return m_refractionIndex;
  }
  
  /**
    * Sets the material's [index of refraction](https://en.wikipedia.org/wiki/Refractive_index).
    * 
    * <table><tr>
    * <td>@image html transparent_material_ior_1.01.png "refractionIndex=1.01"</td>
    * <td>@image html transparent_material_ior_1.03.png "refractionIndex=1.03"</td>
    * <td>@image html transparent_material_ior_1.05.png "refractionIndex=1.05"</td>
    * <td>@image html transparent_material_ior_1.07.png "refractionIndex=1.07"</td>
    * <td>@image html transparent_material_ior_1.09.png "refractionIndex=1.09"</td>
    * <td>@image html transparent_material_ior_1.11.png "refractionIndex=1.11"</td>
    * <td>@image html transparent_material_ior_1.13.png "refractionIndex=1.13"</td>
    * </tr></table>
    */
  inline void setRefractionIndex(double index) {
    m_refractionIndex = index;
  }
  
protected:
  virtual raytracer::Material* toRaytracerMaterial() const;

private:
  double m_refractionIndex;
};
