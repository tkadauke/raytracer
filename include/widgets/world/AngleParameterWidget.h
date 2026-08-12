#pragma once
#include <memory>

#include "widgets/world/AbstractParameterWidget.h"

class AngleParameterWidget : public AbstractParameterWidget {
  Q_OBJECT

public:
  explicit AngleParameterWidget(QWidget* parent = nullptr);
  ~AngleParameterWidget();

  virtual const QVariant value() const;
  virtual void setValue(const QVariant& value);

private slots:
  void recalculate();

private:
  QString type() const;
  void setLabelText(const QString& text) override;

  struct Private;
  std::unique_ptr<Private> p;
};
