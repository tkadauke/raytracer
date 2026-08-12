#pragma once
#include <memory>

#include "widgets/world/AbstractParameterWidget.h"

class StringParameterWidget : public AbstractParameterWidget {
  Q_OBJECT

public:
  explicit StringParameterWidget(QWidget* parent = nullptr);
  ~StringParameterWidget();

  virtual const QVariant value() const;
  virtual void setValue(const QVariant& value);

protected:
  void setLabelText(const QString& text) override;

private:
  struct Private;
  std::unique_ptr<Private> p;
};
