#include "widgets/world/RenderSettingsWidget.h"
#include "RenderSettingsWidget.uic"

struct RenderSettingsWidget::Private {
  Ui::RenderSettingsWidget ui;
};

RenderSettingsWidget::RenderSettingsWidget(QWidget* parent)
  : QWidget(parent),
    p(std::make_unique<Private>())
{
  p->ui.setupUi(this);
  connect(p->ui.m_renderButton, SIGNAL(clicked()), this, SLOT(render()));
  connect(p->ui.m_stopButton, SIGNAL(clicked()), this, SLOT(stop()));
}

RenderSettingsWidget::~RenderSettingsWidget() {
}  

QSize RenderSettingsWidget::resolution() {
  QString resolution = p->ui.m_resolution->currentText();
  auto components = resolution.split("x");
  
  return QSize(components[0].toInt(), components[1].toInt());
}

QString RenderSettingsWidget::sampler() {
  return p->ui.m_samplerType->currentText();
}

int RenderSettingsWidget::samplesPerPixel() {
  return p->ui.m_samplesPerPixel->value();
}

int RenderSettingsWidget::maxRecursionDepth() {
  return p->ui.m_maxRecursionDepth->value();
}

void RenderSettingsWidget::setBusy(bool busy) {
  p->ui.m_resolution->setEnabled(!busy);
  p->ui.m_samplerType->setEnabled(!busy);
  p->ui.m_samplesPerPixel->setEnabled(!busy);
  p->ui.m_maxRecursionDepth->setEnabled(!busy);
  
  p->ui.m_renderButton->setEnabled(!busy);
  p->ui.m_stopButton->setEnabled(busy);
}

void RenderSettingsWidget::setElapsedTime(int milliseconds) {
  int seconds = milliseconds / 1000;
  p->ui.m_timeLabel->setText(
    QString("Elapsed time: %1:%2:%3")
      .arg(seconds / 3600)
      .arg((seconds % 3600) / 60, 2, 10, QLatin1Char('0'))
      .arg(seconds % 60, 2, 10, QLatin1Char('0'))
  );
}

void RenderSettingsWidget::render() {
  emit renderClicked();
}

void RenderSettingsWidget::stop() {
  emit stopClicked();
}

#include "RenderSettingsWidget.moc"
