#include "widgets/CameraParameterWidget.h"

CameraParameterWidget::CameraParameterWidget(QWidget* parent)
    : QWidget(parent) {
}

void CameraParameterWidget::parameterChanged() {
  emit changed();
}
