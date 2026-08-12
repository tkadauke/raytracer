#pragma once
#include <memory>

#include "widgets/world/AbstractParameterWidget.h"

class DoubleParameterWidget : public AbstractParameterWidget {
  Q_OBJECT

public:
  explicit DoubleParameterWidget(QWidget* parent = nullptr);
  ~DoubleParameterWidget();

  const QVariant value() const override;
  void setValue(const QVariant& value) override;

private:
  void updatePropertyConfiguration() override;
  void setLabelText(const QString& text) override;

  struct Private;
  std::unique_ptr<Private> p;
};
