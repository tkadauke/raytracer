#pragma once

#include "widgets/QtDisplay.h"

class Material;

namespace raytracer {
  class Scene;
}

class MaterialDisplayWidget : public QtDisplay {
  Q_OBJECT

public:
  MaterialDisplayWidget(QWidget* parent);
  ~MaterialDisplayWidget();
  
  void setMaterial(Material* material);
  
  virtual QSize sizeHint() const;
  
private:
  raytracer::Scene* sphereOnPlane();

  Material* m_material;
};
