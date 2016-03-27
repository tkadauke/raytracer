#pragma once

#include "widgets/world/AbstractParameterWidget.h"

class IntParameterWidget : public AbstractParameterWidget {
  Q_OBJECT;
  
public:
  explicit IntParameterWidget(QWidget* parent = nullptr);
  ~IntParameterWidget();
  
  virtual void setParameterName(const QString& name);

  virtual const QVariant value() const;
  virtual void setValue(const QVariant& value);

private:
  struct Private;
  std::unique_ptr<Private> p;
};
