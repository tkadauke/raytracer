#pragma once
#include <memory>

#include "widgets/world/AbstractParameterWidget.h"

#include <QStringList>

class ChoiceParameterWidget : public AbstractParameterWidget {
  Q_OBJECT

public:
  explicit ChoiceParameterWidget(QStringList choices, QWidget* parent = nullptr);
  ~ChoiceParameterWidget();

  void setParameterName(const QString& name) override;

  const QVariant value() const override;
  void setValue(const QVariant& value) override;

private:
  void populateChoices();

  struct Private;
  std::unique_ptr<Private> p;
};
