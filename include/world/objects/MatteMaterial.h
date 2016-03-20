#pragma once

#include "world/objects/Material.h"

class Texture;

/**
  * Matte materials have no reflection, or transmission. As the name suggests,
  * they appear matte.
  * 
  * @image html matte_material_red.png "Matte material with red constant texture"
  */
class MatteMaterial : public Material {
  Q_OBJECT;
  Q_PROPERTY(Texture* diffuseTexture READ diffuseTexture WRITE setDiffuseTexture);
  Q_PROPERTY(double ambientCoefficient READ ambientCoefficient WRITE setAmbientCoefficient);
  Q_PROPERTY(double diffuseCoefficient READ diffuseCoefficient WRITE setDiffuseCoefficient);

public:
  /**
    * Constructs the default matte material with no texture and ambient and
    * diffuse coefficients of 1.
    */
  explicit MatteMaterial(Element* parent = nullptr);

  /**
    * @returns the diffuse texture.
    */
  inline Texture* diffuseTexture() const {
    return m_diffuseTexture;
  }
  
  /**
    * Sets the material's diffuse texture.
    * 
    * <table><tr>
    * <td>@image html matte_material_rainbow_red.png</td>
    * <td>@image html matte_material_rainbow_orange.png</td>
    * <td>@image html matte_material_rainbow_yellow.png</td>
    * <td>@image html matte_material_rainbow_green.png</td>
    * <td>@image html matte_material_rainbow_blue.png</td>
    * <td>@image html matte_material_rainbow_indigo.png</td>
    * <td>@image html matte_material_rainbow_violet.png</td>
    * </tr></table>
    */
  inline void setDiffuseTexture(Texture* texture) {
    m_diffuseTexture = texture;
  }
  
  /**
    * @returns the ambient light coefficient.
    */
  inline double ambientCoefficient() const {
    return m_ambientCoefficient;
  }

  /**
    * Sets the ambient light coefficient.
    * 
    * <table><tr>
    * <td>@image html matte_material_ambient_0.0.png "ambientCoefficient=0"</td>
    * <td>@image html matte_material_ambient_0.25.png "ambientCoefficient=0.25"</td>
    * <td>@image html matte_material_ambient_0.5.png "ambientCoefficient=0.5"</td>
    * <td>@image html matte_material_ambient_0.75.png "ambientCoefficient=0.75"</td>
    * <td>@image html matte_material_ambient_1.0.png "ambientCoefficient=1"</td>
    * </tr></table>
    */
  inline void setAmbientCoefficient(double coeff) {
    m_ambientCoefficient = coeff;
  }
  
  /**
    * @returns the diffuse light coefficient.
    */
  inline double diffuseCoefficient() const {
    return m_diffuseCoefficient;
  }
  
  /**
    * Sets the diffuse light coefficient.
    * 
    * <table><tr>
    * <td>@image html matte_material_diffuse_0.0.png "diffuseCoefficient=0"</td>
    * <td>@image html matte_material_diffuse_0.5.png "diffuseCoefficient=0.5"</td>
    * <td>@image html matte_material_diffuse_1.0.png "diffuseCoefficient=1"</td>
    * <td>@image html matte_material_diffuse_1.5.png "diffuseCoefficient=1.5"</td>
    * <td>@image html matte_material_diffuse_2.0.png "diffuseCoefficient=2"</td>
    * </tr></table>
    */
  inline void setDiffuseCoefficient(double coeff) {
    m_diffuseCoefficient = coeff;
  }
  
protected:
  virtual raytracer::Material* toRaytracerMaterial() const;
  
private:
  Texture* m_diffuseTexture;
  double m_ambientCoefficient;
  double m_diffuseCoefficient;
};
