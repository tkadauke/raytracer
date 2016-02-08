#pragma once

#include "widgets/QtDisplay.h"

class Scene;
class Element;

namespace raytracer {
  class Camera;
}

class Display : public QtDisplay {
  Q_OBJECT

public:
  Display(QWidget* parent);
  ~Display();
  
  void setScene(Scene* scene);
};
