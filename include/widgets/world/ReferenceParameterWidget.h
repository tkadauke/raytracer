#pragma once

#include "widgets/world/AbstractParameterWidget.h"

class ReferenceParameterWidget : public AbstractParameterWidget {
  Q_OBJECT
  
public:
  ReferenceParameterWidget(const QString& baseClassName, Element* root, QWidget* parent = nullptr);
  ~ReferenceParameterWidget();
  
  virtual void setParameterName(const QString& name);

  virtual const QVariant value();
  virtual void setValue(const QVariant& value);

protected:
  void fillComboBox(Element* root);

private:
  struct Private;
  Private* p;
};
