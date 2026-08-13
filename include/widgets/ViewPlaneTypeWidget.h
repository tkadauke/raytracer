#pragma once

#include "widgets/TypeSelectorWidget.h"

#include <memory>
#include <string>

class ViewPlaneTypeWidget : public TypeSelectorWidget {
  Q_OBJECT

public:
  explicit ViewPlaneTypeWidget(QWidget* parent = nullptr);
  ~ViewPlaneTypeWidget();

  std::string type() const;

private:
  struct Private;
  std::unique_ptr<Private> p;
};
