#ifndef VECTOR_PARAMETER_WIDGET_H
#define VECTOR_PARAMETER_WIDGET_H

#include "core/math/Vector.h"
#include "widgets/world/AbstractParameterWidget.h"

class VectorParameterWidget : public AbstractParameterWidget {
  Q_OBJECT
  
public:
  VectorParameterWidget(QWidget* parent = nullptr);
  ~VectorParameterWidget();

  virtual void setParameterName(const QString& name);

  virtual const QVariant value();
  virtual void setValue(const QVariant& value);

protected:
  Vector3d vector() const;
  void setVector(const Vector3d& vector);

private:
  struct Private;
  Private* p;
};

#endif
