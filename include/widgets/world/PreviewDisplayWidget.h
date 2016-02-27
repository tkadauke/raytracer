#pragma once

#include "widgets/QtDisplay.h"

class Material;

namespace raytracer {
  class Scene;
}

class PreviewDisplayWidget : public QtDisplay {
  Q_OBJECT

public:
  PreviewDisplayWidget(QWidget* parent);
  ~PreviewDisplayWidget();
  
  void setMaterial(Material* material);
  
  virtual QSize sizeHint() const;
  
private:
  raytracer::Scene* sphereOnPlane();

  Material* m_material;
};
