#pragma once

#include "widgets/QtDisplay.h"
#include "core/Color.h"

class Material;
class Scene;
class Camera;

namespace raytracer {
  class Scene;
}

class PreviewDisplayWidget : public QtDisplay {
  Q_OBJECT;

public:
  PreviewDisplayWidget(QWidget* parent);
  ~PreviewDisplayWidget();
  
  void clear();
  void setMaterial(Material* material, Scene* s);
  void setCamera(Camera* camera, Scene* scene);
  
  virtual QSize sizeHint() const;
  
private:
  void updateScene(const std::function<void()>& setup);
  raytracer::Scene* sphereOnPlane(Material* material, Scene* s) const;
};
