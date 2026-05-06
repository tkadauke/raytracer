#include "raytracer/samplers/SamplerFactory.h"
#include "raytracer/viewplanes/ViewPlaneFactory.h"
#include "widgets/world/RenderSettingsWidget.h"
#include "ui_RenderSettingsWidget.h"

#include <QThread>

struct RenderSettingsWidget::Private {
  Ui::RenderSettingsWidget ui;
};

RenderSettingsWidget::RenderSettingsWidget(QWidget* parent)
  : QWidget(parent),
    p(std::make_unique<Private>())
{
  p->ui.setupUi(this);

  auto ids = raytracer::SamplerFactory::self().identifiers();
  for (const auto& id : ids) {
    p->ui.samplerType->addItem(QString(id.c_str()).replace("Sampler", ""));
  }

  p->ui.samplerType->setCurrentText("Regular");

  ids = raytracer::ViewPlaneFactory::self().identifiers();
  for (const auto& id : ids) {
    p->ui.viewPlaneType->addItem(QString(id.c_str()));
  }

  p->ui.viewPlaneType->setCurrentText("PointInterlacedViewPlane");

  p->ui.renderThreads->setValue(QThread::idealThreadCount());
  p->ui.queueSize->setValue(QThread::idealThreadCount() * 8);

  connect(p->ui.renderButton, SIGNAL(clicked()), this, SLOT(render()));
  connect(p->ui.stopButton, SIGNAL(clicked()), this, SLOT(stop()));
  connect(p->ui.engineType, SIGNAL(currentTextChanged(const QString&)), this, SLOT(engineChanged()));

  // Initial visibility: defaults to Raytracer, so hide the wireframe-
  // only frame.
  engineChanged();
}

RenderSettingsWidget::~RenderSettingsWidget() {
}

QSize RenderSettingsWidget::resolution() const {
  QString resolution = p->ui.resolution->currentText();
  auto components = resolution.split("x");

  return QSize(components[0].toInt(), components[1].toInt());
}

QString RenderSettingsWidget::sampler() const {
  return p->ui.samplerType->currentText();
}

QString RenderSettingsWidget::viewPlane() const {
  return p->ui.viewPlaneType->currentText();
}

QString RenderSettingsWidget::engine() const {
  return p->ui.engineType->currentText();
}

int RenderSettingsWidget::samplesPerPixel() const {
  return p->ui.samplesPerPixel->value();
}

int RenderSettingsWidget::maxRecursionDepth() const {
  return p->ui.maxRecursionDepth->value();
}

int RenderSettingsWidget::renderThreads() const {
  return p->ui.renderThreads->value();
}

int RenderSettingsWidget::queueSize() const {
  return p->ui.queueSize->value();
}

int RenderSettingsWidget::lod() const {
  return p->ui.lod->value();
}

void RenderSettingsWidget::engineChanged() {
  // Show the engine-specific frame; hide the others. Resolution +
  // engine selector + progress indicators stay visible regardless.
  bool isWireframe = (engine() == "Wireframe");
  p->ui.raytracerFrame->setVisible(!isWireframe);
  p->ui.wireframeFrame->setVisible(isWireframe);
}

bool RenderSettingsWidget::showProgressIndicators() const {
  return p->ui.showProgressIndicators->isChecked();
}

void RenderSettingsWidget::setBusy(bool busy) {
  p->ui.resolution->setEnabled(!busy);
  p->ui.viewPlaneType->setEnabled(!busy);
  p->ui.samplerType->setEnabled(!busy);
  p->ui.engineType->setEnabled(!busy);
  p->ui.samplesPerPixel->setEnabled(!busy);
  p->ui.maxRecursionDepth->setEnabled(!busy);
  p->ui.renderThreads->setEnabled(!busy);
  p->ui.queueSize->setEnabled(!busy);
  p->ui.lod->setEnabled(!busy);
  p->ui.showProgressIndicators->setEnabled(!busy);

  p->ui.renderButton->setEnabled(!busy);
  p->ui.stopButton->setEnabled(busy);
}

void RenderSettingsWidget::setElapsedTime(int milliseconds) {
  int seconds = milliseconds / 1000;
  p->ui.timeLabel->setText(
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

