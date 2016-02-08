#pragma once

#include "core/Color.h"
#include "widgets/world/AbstractParameterWidget.h"

class ColorParameterWidget : public AbstractParameterWidget {
  Q_OBJECT
  
public:
  ColorParameterWidget(QWidget* parent = nullptr);
  ~ColorParameterWidget();

  virtual void setParameterName(const QString& name);

  virtual const QVariant value();
  virtual void setValue(const QVariant& value);
  
protected slots:
  void selectorClicked();

protected:
  Colord color() const;
  void setColor(const Colord& color);

private:
  Colord qColorToColord(const QColor& color);
  QColor colordToQColor(const Colord& color);

  struct Private;
  Private* p;
};
