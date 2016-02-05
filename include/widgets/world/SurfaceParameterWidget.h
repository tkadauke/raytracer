#ifndef SURFACE_PARAMETER_WIDGET_H
#define SURFACE_PARAMETER_WIDGET_H

#include <QWidget>

#include "core/math/Vector.h"
#include "core/math/Quaternion.h"

class Element;
class Surface;

class SurfaceParameterWidget : public QWidget {
  Q_OBJECT
  
public:
  SurfaceParameterWidget(QWidget* parent = 0);
  ~SurfaceParameterWidget();
  
  bool visible() const;
  void setVisible(bool visible);
  
  Vector3d position() const;
  void setPosition(const Vector3d& position);
  
  Vector3d scale() const;
  void setScale(const Vector3d& scale);
  
  Quaterniond orientation() const;
  void setOrientation(const Vector3d& orientation);

  virtual void applyTo(Element* element);
  virtual void getFrom(Element* element);

signals:
  void changed();

private slots:
  void parameterChanged();

private:
  struct Private;
  Private* p;
};

#endif