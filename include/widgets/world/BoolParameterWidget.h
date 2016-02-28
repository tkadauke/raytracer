#pragma once

#include "widgets/world/AbstractParameterWidget.h"

class BoolParameterWidget : public AbstractParameterWidget {
  Q_OBJECT;
  
public:
  BoolParameterWidget(QWidget* parent = nullptr);
  ~BoolParameterWidget();
  
  virtual void setParameterName(const QString& name);
  
  virtual const QVariant value();
  virtual void setValue(const QVariant& value);

private:
  struct Private;
  std::unique_ptr<Private> p;
};
