#pragma once
#include <memory>

#include "core/math/Vector.h"
#include "widgets/world/AbstractParameterWidget.h"

class VectorParameterWidget : public AbstractParameterWidget {
  Q_OBJECT

public:
  explicit VectorParameterWidget(QWidget* parent = nullptr);
  ~VectorParameterWidget();

  virtual const QVariant value() const;
  virtual void setValue(const QVariant& value);

protected:
  Vector3d vector() const;
  void setVector(const Vector3d& vector);
  void setLabelText(const QString& text) override;

private:
  struct Private;
  std::unique_ptr<Private> p;
};
