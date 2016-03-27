#pragma once

#include "widgets/world/AbstractParameterWidget.h"

class ReferenceParameterWidget : public AbstractParameterWidget {
  Q_OBJECT;
  
public:
  explicit ReferenceParameterWidget(const QString& baseClassName, Element* root, QWidget* parent = nullptr);
  ~ReferenceParameterWidget();
  
  virtual void setParameterName(const QString& name);

  virtual const QVariant value() const;
  virtual void setValue(const QVariant& value);

protected:
  void fillComboBox(Element* root);

private:
  QVariant makeVariant(Element* e);
  
  struct Private;
  std::unique_ptr<Private> p;
};
