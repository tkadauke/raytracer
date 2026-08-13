#pragma once

#include "widgets/TypeSelectorWidget.h"

#include <memory>
#include <string>

class CameraTypeWidget : public TypeSelectorWidget {
  Q_OBJECT

public:
  explicit CameraTypeWidget(QWidget* parent = nullptr);
  ~CameraTypeWidget();

  std::string type() const;

private:
  struct Private;
  std::unique_ptr<Private> p;
};
