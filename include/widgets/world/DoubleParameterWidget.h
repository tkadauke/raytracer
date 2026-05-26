#pragma once
#include <memory>

#include "widgets/world/AbstractParameterWidget.h"

class DoubleParameterWidget : public AbstractParameterWidget {
  Q_OBJECT

public:
  explicit DoubleParameterWidget(QWidget* parent = nullptr);
  ~DoubleParameterWidget();

  void setParameterName(const QString& name) override;

  const QVariant value() const override;
  void setValue(const QVariant& value) override;

private:
  void updatePropertyConfiguration() override;

  struct Private;
  std::unique_ptr<Private> p;
};
