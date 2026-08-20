#include "widgets/CameraParameterWidget.h"

#include <QLabel>
#include <QSlider>

CameraParameterWidget::CameraParameterWidget(QWidget* parent)
    : QWidget(parent) {
}

void CameraParameterWidget::parameterChanged() {
  emit changed();
}

void CameraParameterWidget::connectDegreeSlider(QSlider* slider, QLabel* label) {
  // Update the value label when the slider moves.  Use a lambda rather than
  // &QLabel::setNum to avoid the int/double overload ambiguity in Qt 6.
  connect(slider, &QSlider::valueChanged, this, [label](int v) { label->setNum(v); });
  connect(slider, SIGNAL(valueChanged(int)), this, SLOT(parameterChanged()));
}
