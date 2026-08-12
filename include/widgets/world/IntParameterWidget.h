#pragma once
#include <memory>

#include "widgets/world/AbstractParameterWidget.h"

class IntParameterWidget : public AbstractParameterWidget {
  Q_OBJECT

public:
  explicit IntParameterWidget(QWidget* parent = nullptr);
  ~IntParameterWidget();

  const QVariant value() const override;
  void setValue(const QVariant& value) override;

private:
  void updatePropertyConfiguration() override;
  void setLabelText(const QString& text) override;

  struct Private;
  std::unique_ptr<Private> p;
};
