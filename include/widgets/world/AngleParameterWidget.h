#pragma once

#include "widgets/world/AbstractParameterWidget.h"

class AngleParameterWidget : public AbstractParameterWidget {
  Q_OBJECT;
  
public:
  AngleParameterWidget(QWidget* parent = nullptr);
  ~AngleParameterWidget();
  
  virtual void setParameterName(const QString& name);

  virtual const QVariant value();
  virtual void setValue(const QVariant& value);

private slots:
  void recalculate();

private:
  QString type();
  
  struct Private;
  std::unique_ptr<Private> p;
};
