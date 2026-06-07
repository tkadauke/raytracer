#include "engine/raster/OpenGLRasterizer.h"
#include "render/RayFamilyQueuePolicy.h"
#include "render/samplers/SamplerFactory.h"
#include "render/viewplanes/ViewPlaneFactory.h"
#include "widgets/world/RenderSettingsWidget.h"
#include "ui_RenderSettingsWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QSpinBox>
#include <QThread>

struct RenderSettingsWidget::Private {
  Ui::RenderSettingsWidget ui;
  QLabel* rasterBackendStatus{nullptr};

  bool openGLBackendSelected() const {
    return ui.rasterBackend->currentText() == QStringLiteral("OpenGL");
  }

  void updateRasterBackendStatus(bool visible) {
    if (!rasterBackendStatus) {
      return;
    }
    rasterBackendStatus->setVisible(visible);
    if (visible) {
      rasterBackendStatus->setText(
        QString::fromStdString(engine::raster::OpenGLRasterizer::statusMessage()));
    }
  }
};

RenderSettingsWidget::RenderSettingsWidget(QWidget* parent)
    : QWidget(parent),
      p(std::make_unique<Private>()) {
  p->ui.setupUi(this);

  auto ids = render::SamplerFactory::self().identifiers();
  for (const auto& id : ids) {
    p->ui.samplerType->addItem(QString(id.c_str()).replace("Sampler", ""));
  }

  p->ui.samplerType->setCurrentText("Regular");

  ids = render::ViewPlaneFactory::self().identifiers();
  for (const auto& id : ids) {
    p->ui.viewPlaneType->addItem(QString(id.c_str()));
  }

  p->ui.viewPlaneType->setCurrentText("PointInterlacedViewPlane");

  p->rasterBackendStatus = new QLabel(p->ui.wireframeFrame);
  p->rasterBackendStatus->setObjectName(QStringLiteral("rasterBackendStatus"));
  p->rasterBackendStatus->setWordWrap(true);
  p->rasterBackendStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
  p->rasterBackendStatus->setVisible(false);
  p->ui.wireframeGridLayout->addWidget(p->rasterBackendStatus,
                                       p->ui.wireframeGridLayout->rowCount(), 0, 1, 2);

  p->ui.renderThreads->setValue(QThread::idealThreadCount());
  p->ui.queueSize->setValue(queueSize());
  p->ui.label_5->setVisible(false);
  p->ui.viewPlaneType->setVisible(false);
  p->ui.label_6->setVisible(false);
  p->ui.renderThreads->setVisible(false);
  p->ui.label_7->setVisible(false);
  p->ui.queueSize->setVisible(false);

  connect(p->ui.renderButton, SIGNAL(clicked()), this, SLOT(render()));
  connect(p->ui.stopButton, SIGNAL(clicked()), this, SLOT(stop()));
  connect(p->ui.engineType, SIGNAL(currentTextChanged(const QString&)), this,
          SLOT(engineChanged()));
  connect(p->ui.rasterBackend, SIGNAL(currentTextChanged(const QString&)), this,
          SLOT(updateEngineControls()));
  connect(p->ui.rasterShadowMaps, SIGNAL(toggled(bool)), this, SLOT(updateEngineControls()));
  for (auto* comboBox : findChildren<QComboBox*>()) {
    connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &RenderSettingsWidget::settingsChanged);
  }
  for (auto* spinBox : findChildren<QSpinBox*>()) {
    connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &RenderSettingsWidget::settingsChanged);
  }
  for (auto* spinBox : findChildren<QDoubleSpinBox*>()) {
    connect(spinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &RenderSettingsWidget::settingsChanged);
  }
  for (auto* checkBox : findChildren<QCheckBox*>()) {
    connect(checkBox, &QCheckBox::toggled, this, &RenderSettingsWidget::settingsChanged);
  }

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
  const QSize size = resolution();
  return render::RayFamilyQueuePolicy(size.width(), size.height(), samplesPerPixel(),
                                      renderThreads())
    .queueSize();
}

int RenderSettingsWidget::lod() const {
  return p->ui.lod->value();
}

QString RenderSettingsWidget::rasterBackend() const {
  return p->ui.rasterBackend->currentText();
}

int RenderSettingsWidget::msaaSamples() const {
  return p->ui.rasterMsaaSamples->currentText().toInt();
}

QString RenderSettingsWidget::msaaShadingMode() const {
  return p->ui.rasterMsaaShadingMode->currentText();
}

QString RenderSettingsWidget::postProcessAA() const {
  return p->ui.rasterPostProcessAA->currentText();
}

bool RenderSettingsWidget::shadowMapsEnabled() const {
  return p->ui.rasterShadowMaps->isChecked();
}

int RenderSettingsWidget::shadowMapSize() const {
  return p->ui.rasterShadowMapSize->value();
}

int RenderSettingsWidget::shadowCascadeCount() const {
  return p->ui.rasterShadowCascadeCount->value();
}

double RenderSettingsWidget::shadowCascadeSplitLambda() const {
  return p->ui.rasterShadowCascadeSplitLambda->value();
}

double RenderSettingsWidget::shadowBias() const {
  return p->ui.rasterShadowBias->value();
}

double RenderSettingsWidget::shadowSlopeBias() const {
  return p->ui.rasterShadowSlopeBias->value();
}

int RenderSettingsWidget::shadowFilterRadius() const {
  return p->ui.rasterShadowFilterRadius->value();
}

QString RenderSettingsWidget::shadowFilterMode() const {
  return p->ui.rasterShadowFilterMode->currentText();
}

RenderWidget::DisplayMode RenderSettingsWidget::displayMode() const {
  const QString mode = p->ui.displayUpdateMode->currentText();
  if (mode == "Completed tiles")
    return RenderWidget::DisplayMode::CompletedTilePublishing;
  if (mode == "Double buffer")
    return RenderWidget::DisplayMode::DoubleBuffer;
  return RenderWidget::DisplayMode::PeriodicUpdate;
}

void RenderSettingsWidget::engineChanged() {
  if (engine() == "Raytracer" || engine() == "Path Tracer" || engine() == "Wavefront") {
    p->ui.displayUpdateMode->setCurrentText("Periodic update");
    p->ui.showProgressIndicators->setChecked(true);
  } else {
    p->ui.displayUpdateMode->setCurrentText("Double buffer");
    p->ui.showProgressIndicators->setChecked(false);
  }

  updateEngineControls();
}

void RenderSettingsWidget::updateEngineControls() {
  // Show the engine-specific frame; hide the others. Resolution +
  // engine selector + progress indicators stay visible regardless.
  // Rasterizer shares Wireframe's LOD knob, and adds raster-only quality controls.
  const QString eng = engine();
  const bool isRayFamily = (eng == "Raytracer" || eng == "Path Tracer" || eng == "Wavefront");
  const bool isRasterizer = (eng == "Rasterizer");
  const bool showShadowDetails = isRasterizer && shadowMapsEnabled();
  p->ui.raytracerFrame->setVisible(isRayFamily);
  p->ui.wireframeFrame->setVisible(!isRayFamily);
  p->ui.label_rasterBackend->setVisible(isRasterizer);
  p->ui.rasterBackend->setVisible(isRasterizer);
  p->ui.label_rasterMsaa->setVisible(isRasterizer);
  p->ui.rasterMsaaSamples->setVisible(isRasterizer);
  p->ui.label_rasterMsaaShading->setVisible(isRasterizer);
  p->ui.rasterMsaaShadingMode->setVisible(isRasterizer);
  p->ui.label_rasterPostAA->setVisible(isRasterizer);
  p->ui.rasterPostProcessAA->setVisible(isRasterizer);
  p->ui.rasterShadowMaps->setVisible(isRasterizer);
  p->ui.label_rasterShadowMapSize->setVisible(showShadowDetails);
  p->ui.rasterShadowMapSize->setVisible(showShadowDetails);
  p->ui.label_rasterShadowCascadeCount->setVisible(showShadowDetails);
  p->ui.rasterShadowCascadeCount->setVisible(showShadowDetails);
  p->ui.label_rasterShadowCascadeSplitLambda->setVisible(showShadowDetails);
  p->ui.rasterShadowCascadeSplitLambda->setVisible(showShadowDetails);
  p->ui.label_rasterShadowBias->setVisible(showShadowDetails);
  p->ui.rasterShadowBias->setVisible(showShadowDetails);
  p->ui.label_rasterShadowSlopeBias->setVisible(showShadowDetails);
  p->ui.rasterShadowSlopeBias->setVisible(showShadowDetails);
  p->ui.label_rasterShadowFilterRadius->setVisible(showShadowDetails);
  p->ui.rasterShadowFilterRadius->setVisible(showShadowDetails);
  p->ui.label_rasterShadowFilterMode->setVisible(showShadowDetails);
  p->ui.rasterShadowFilterMode->setVisible(showShadowDetails);
  p->updateRasterBackendStatus(isRasterizer && p->openGLBackendSelected());
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
  p->ui.rasterBackend->setEnabled(!busy);
  p->ui.rasterMsaaSamples->setEnabled(!busy);
  p->ui.rasterMsaaShadingMode->setEnabled(!busy);
  p->ui.rasterPostProcessAA->setEnabled(!busy);
  p->ui.rasterShadowMaps->setEnabled(!busy);
  p->ui.rasterShadowMapSize->setEnabled(!busy);
  p->ui.rasterShadowCascadeCount->setEnabled(!busy);
  p->ui.rasterShadowCascadeSplitLambda->setEnabled(!busy);
  p->ui.rasterShadowBias->setEnabled(!busy);
  p->ui.rasterShadowSlopeBias->setEnabled(!busy);
  p->ui.rasterShadowFilterRadius->setEnabled(!busy);
  p->ui.rasterShadowFilterMode->setEnabled(!busy);
  p->ui.displayUpdateMode->setEnabled(!busy);
  p->ui.showProgressIndicators->setEnabled(!busy);

  p->ui.renderButton->setEnabled(!busy);
  p->ui.stopButton->setEnabled(busy);
}

void RenderSettingsWidget::setElapsedTime(int milliseconds) {
  int seconds = milliseconds / 1000;
  p->ui.timeLabel->setText(QString("Elapsed time: %1:%2:%3")
                             .arg(seconds / 3600)
                             .arg((seconds % 3600) / 60, 2, 10, QLatin1Char('0'))
                             .arg(seconds % 60, 2, 10, QLatin1Char('0')));
}

void RenderSettingsWidget::render() {
  emit renderClicked();
}

void RenderSettingsWidget::stop() {
  emit stopClicked();
}
