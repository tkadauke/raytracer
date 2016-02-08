#pragma once

#include "widgets/world/AbstractParameterWidget.h"

class DoubleParameterWidget : public AbstractParameterWidget {
  Q_OBJECT
  
public:
  DoubleParameterWidget(QWidget* parent = nullptr);
  ~DoubleParameterWidget();
  
  virtual void setParameterName(const QString& name);

  virtual const QVariant value();
  virtual void setValue(const QVariant& value);

private:
  struct Private;
  Private* p;
};
