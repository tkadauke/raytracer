#ifndef MATERIAL_H
#define MATERIAL_H

#include "Color.h"

class Material {
public:
  inline Material()
    : m_highlightColor(1, 1, 1), m_refractionIndex(1)
  {
  }
  
  inline Material(const Colord& color)
    : m_diffuseColor(color), m_highlightColor(1, 1, 1), m_refractionIndex(1)
  {
  }
  
  inline void setDiffuseColor(const Colord& color) { m_diffuseColor = color; }
  inline const Colord& diffuseColor() const { return m_diffuseColor; }

  inline void setHighlightColor(const Colord& color) { m_highlightColor = color; }
  inline const Colord& highlightColor() const { return m_highlightColor; }

  inline void setSpecularColor(const Colord& color) { m_specularColor = color; }
  inline const Colord& specularColor() const { return m_specularColor; }
  
  inline bool isReflective() const { return m_specularColor != Colord::black; }

  inline void setAbsorbanceColor(const Colord& color) { m_absorbanceColor = color; }
  inline const Colord& absorbanceColor() const { return m_absorbanceColor; }

  inline void setRefractionIndex(double index) { m_refractionIndex = index; }
  inline double refractionIndex() const { return m_refractionIndex; }

  inline bool isRefractive() const { return m_absorbanceColor != Colord::black; }
  
  inline bool isSpecular() const { return isReflective() || isRefractive(); }

private:
  Colord m_diffuseColor, m_highlightColor, m_specularColor, m_absorbanceColor;
  double m_refractionIndex;
};

#endif
