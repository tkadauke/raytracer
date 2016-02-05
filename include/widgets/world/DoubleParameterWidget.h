#ifndef DOUBLE_PARAMETER_WIDGET_H
#define DOUBLE_PARAMETER_WIDGET_H

#include "widgets/world/AbstractParameterWidget.h"

class DoubleParameterWidget : public AbstractParameterWidget {
  Q_OBJECT
  
public:
  DoubleParameterWidget(QWidget* parent = 0);
  ~DoubleParameterWidget();
  
  virtual void setParameterName(const QString& name);

  virtual const QVariant value();
  virtual void setValue(const QVariant& value);

private:
  struct Private;
  Private* p;
};

#endif
