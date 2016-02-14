#pragma once

#include "widgets/world/AbstractParameterWidget.h"

class StringParameterWidget : public AbstractParameterWidget {
  Q_OBJECT
  
public:
  StringParameterWidget(QWidget* parent = nullptr);
  ~StringParameterWidget();
  
  virtual void setParameterName(const QString& name);

  virtual const QVariant value();
  virtual void setValue(const QVariant& value);

private:
  struct Private;
  Private* p;
};