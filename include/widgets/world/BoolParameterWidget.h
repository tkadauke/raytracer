#pragma once
#include <memory>

#include "widgets/world/AbstractParameterWidget.h"

class BoolParameterWidget : public AbstractParameterWidget {
  Q_OBJECT

public:
  explicit BoolParameterWidget(QWidget* parent = nullptr);
  ~BoolParameterWidget();

  virtual const QVariant value() const;
  virtual void setValue(const QVariant& value);

protected:
  void setLabelText(const QString& text) override;

private:
  struct Private;
  std::unique_ptr<Private> p;
};
