#include "engine/graph/RenderGraphTypes.h"
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
#include <QSignalBlocker>
#include <QSpinBox>
#include <QThread>

#include <algorithm>

struct RenderSettingsWidget::Private {
  Ui::RenderSettingsWidget ui;
  QLabel* rasterBackendStatus{nullptr};
  bool samplerDefaultManaged{true};
  bool updatingSamplerDefault{false};
  bool samplesPerPixelDefaultManaged{true};
  bool updatingSamplesPerPixelDefault{false};

  void setComboBoxText(QComboBox* comboBox, const QString& text) const {
    const int index = comboBox->findText(text);
    if (index >= 0) {
      comboBox->setCurrentIndex(index);
    }
  }

  void setSpinBoxValue(QSpinBox* spinBox, int value) const {
    spinBox->setValue(std::clamp(value, spinBox->minimum(), spinBox->maximum()));
  }

  void setDoubleSpinBoxValue(QDoubleSpinBox* spinBox, double value) const {
    spinBox->setValue(std::clamp(value, spinBox->minimum(), spinBox->maximum()));
  }

  bool openGLBackendSelected() const {
    return ui.rasterBackend->currentText() == QStringLiteral("OpenGL");
  }

  bool isPathTracerIntegrator(const engine::graph::RenderRaytracerOptions& options) const {
    const auto integrator = options.integrator();
    return integrator &&
           (*integrator == "pathtracer" || *integrator == "path_tracer" || *integrator == "pt");
  }

  QString engineText(const engine::graph::RenderIntent& intent) const {
    using engine::graph::RenderExecutorPreference;
    switch (intent.defaultExecutor) {
    case RenderExecutorPreference::PathTracer:
      return QStringLiteral("Path Tracer");
    case RenderExecutorPreference::Wavefront:
      return isPathTracerIntegrator(intent.engineOptions.raytracer())
               ? QStringLiteral("Path Tracer")
               : QStringLiteral("Raytracer");
    case RenderExecutorPreference::Rasterizer:
      return QStringLiteral("Rasterizer");
    case RenderExecutorPreference::Wireframe:
      return QStringLiteral("Wireframe");
    case RenderExecutorPreference::Raytracer:
      if (isPathTracerIntegrator(intent.engineOptions.raytracer())) {
        return QStringLiteral("Path Tracer");
      }
      return QStringLiteral("Raytracer");
    }
    return QStringLiteral("Raytracer");
  }

  QString pathTracingScheduleText(const engine::graph::RenderIntent& intent) const {
    using engine::graph::RenderExecutorPreference;
    if (intent.defaultExecutor == RenderExecutorPreference::Raytracer &&
        isPathTracerIntegrator(intent.engineOptions.raytracer())) {
      return QStringLiteral("Scalar");
    }
    return QStringLiteral("Wavefront");
  }

  QString postProcessAAText(engine::graph::RenderPostProcessAA aa) const {
    using engine::graph::RenderPostProcessAA;
    switch (aa) {
    case RenderPostProcessAA::FXAA:
      return QStringLiteral("FXAA");
    case RenderPostProcessAA::SMAA:
      return QStringLiteral("SMAA");
    case RenderPostProcessAA::TAA:
      return QStringLiteral("TAA");
    case RenderPostProcessAA::None:
      return QStringLiteral("None");
    }
    return QStringLiteral("None");
  }

  QString denoiserText(const engine::graph::RenderRaytracerOptions& options) const {
    if (options.denoiser()) {
      if (*options.denoiser() == "none") {
        return QStringLiteral("None");
      }
      if (*options.denoiser() == "box") {
        return QStringLiteral("Box");
      }
      if (*options.denoiser() == "bilateral") {
        return QStringLiteral("Bilateral");
      }
    }
    if (options.denoiseColorSigma()) {
      return QStringLiteral("Bilateral");
    }
    if (options.denoiseRadius()) {
      return QStringLiteral("Box");
    }
    return QStringLiteral("Scene settings");
  }

  QString intersectionBackendText(const engine::graph::RenderRaytracerOptions& options) const {
    if (options.tracingBackend()) {
      const QString backend = QString::fromLatin1(options.tracingBackend()->id());
      if (backend == QStringLiteral("cpu")) {
        return QStringLiteral("CPU");
      }
      if (backend == QStringLiteral("gpu")) {
        return QStringLiteral("GPU");
      }
      return QStringLiteral("Auto");
    }
    if (!options.intersectionBackend()) {
      return QStringLiteral("Auto");
    }
    const QString backend = QString::fromLatin1(options.intersectionBackend()->id());
    if (backend == QStringLiteral("cpu")) {
      return QStringLiteral("CPU");
    }
    if (backend == QStringLiteral("gpu")) {
      return QStringLiteral("GPU");
    }
    return QStringLiteral("Auto");
  }

  QString tracingExecutionText(const engine::graph::RenderRaytracerOptions& options) const {
    using engine::graph::TracingExecutionPreference;
    const auto execution = options.tracingExecution().value_or(TracingExecutionPreference::Auto);
    switch (execution) {
    case TracingExecutionPreference::Auto:
      return QStringLiteral("Auto");
    case TracingExecutionPreference::CPU:
      return QStringLiteral("CPU");
    case TracingExecutionPreference::Hybrid:
      return QStringLiteral("Hybrid");
    case TracingExecutionPreference::GPU:
      return QStringLiteral("GPU");
    }
    return QStringLiteral("Auto");
  }

  void applyRaytracerOptions(const engine::graph::RenderRaytracerOptions& options) {
    samplerDefaultManaged = !options.sampler().has_value();
    samplesPerPixelDefaultManaged = !options.samplesPerPixel().has_value();

    if (options.sampler()) {
      updatingSamplerDefault = true;
      setComboBoxText(ui.samplerType, QString::fromStdString(*options.sampler()));
      updatingSamplerDefault = false;
    } else {
      selectSamplerDefaultForEngine(ui.engineType->currentText());
    }
    if (options.samplesPerPixel()) {
      updatingSamplesPerPixelDefault = true;
      setSpinBoxValue(ui.samplesPerPixel, *options.samplesPerPixel());
      updatingSamplesPerPixelDefault = false;
    } else {
      selectSamplesPerPixelDefaultForEngine(ui.engineType->currentText());
    }
    if (options.maximumRecursionDepth()) {
      setSpinBoxValue(ui.maxRecursionDepth, *options.maximumRecursionDepth());
    }
    setSpinBoxValue(ui.pathTracerDirectLightSamples, options.directLightSamples().value_or(1));
    if (options.maximumThreads()) {
      setSpinBoxValue(ui.renderThreads, *options.maximumThreads());
    }
    if (options.queueSize()) {
      setSpinBoxValue(ui.queueSize, *options.queueSize());
    }
    if (options.viewPlane()) {
      setComboBoxText(ui.viewPlaneType, QString::fromStdString(*options.viewPlane()));
    }
    setComboBoxText(ui.tracingExecution, tracingExecutionText(options));
    setComboBoxText(ui.wavefrontIntersectionBackend, intersectionBackendText(options));
    const QString denoiser = denoiserText(options);
    setComboBoxText(ui.rayDenoiser, denoiser);
    if (options.denoiseRadius()) {
      setSpinBoxValue(ui.rayDenoiseRadius, *options.denoiseRadius());
    } else {
      setSpinBoxValue(ui.rayDenoiseRadius, denoiser == QStringLiteral("Box") ? 1 : 2);
    }
    if (options.denoiseColorSigma()) {
      setDoubleSpinBoxValue(ui.rayDenoiseColorSigma, *options.denoiseColorSigma());
    } else {
      setDoubleSpinBoxValue(ui.rayDenoiseColorSigma, 0.1);
    }
  }

  void applyRasterizerOptions(const engine::graph::RenderIntent& intent) {
    const auto& options = intent.engineOptions.rasterizer();
    setComboBoxText(ui.rasterPostProcessAA, postProcessAAText(intent.postProcessAA));
    ui.rasterShadowMaps->setChecked(intent.enablePreviewShadows);
    if (options.backend()) {
      setComboBoxText(ui.rasterBackend, QString::fromUtf8(options.backend()->displayName()));
    }
    if (options.lod()) {
      setSpinBoxValue(ui.lod, *options.lod());
    }
    if (options.msaaSamples()) {
      setComboBoxText(ui.rasterMsaaSamples, QString::number(*options.msaaSamples()));
    }
    if (options.msaaShadingMode()) {
      setComboBoxText(ui.rasterMsaaShadingMode, *options.msaaShadingMode() == "per_fragment"
                                                  ? QStringLiteral("Per fragment")
                                                  : QStringLiteral("Per sample"));
    }
    if (options.maximumThreads()) {
      setSpinBoxValue(ui.renderThreads, *options.maximumThreads());
    }
    if (options.shadowMapSize()) {
      setSpinBoxValue(ui.rasterShadowMapSize, *options.shadowMapSize());
    }
    if (options.shadowCascadeCount()) {
      setSpinBoxValue(ui.rasterShadowCascadeCount, *options.shadowCascadeCount());
    }
    if (options.shadowCascadeSplitLambda()) {
      setDoubleSpinBoxValue(ui.rasterShadowCascadeSplitLambda, *options.shadowCascadeSplitLambda());
    }
    if (options.shadowBias()) {
      setDoubleSpinBoxValue(ui.rasterShadowBias, *options.shadowBias());
    }
    if (options.shadowSlopeBias()) {
      setDoubleSpinBoxValue(ui.rasterShadowSlopeBias, *options.shadowSlopeBias());
    }
    if (options.shadowFilterRadius()) {
      setSpinBoxValue(ui.rasterShadowFilterRadius, *options.shadowFilterRadius());
    }
    if (options.shadowFilterMode()) {
      setComboBoxText(ui.rasterShadowFilterMode, *options.shadowFilterMode() == "pcss"
                                                   ? QStringLiteral("PCSS")
                                                   : QStringLiteral("PCF"));
    }
  }

  void applyWireframeOptions(const engine::graph::RenderWireframeOptions& options) {
    if (options.lod()) {
      setSpinBoxValue(ui.lod, *options.lod());
    }
  }

  void selectSamplerDefaultForEngine(const QString& engine) {
    if (!samplerDefaultManaged) {
      return;
    }

    const bool pathTracingSelected = engine == QStringLiteral("Path Tracer");
    const QString sampler =
      pathTracingSelected ? QStringLiteral("Halton") : QStringLiteral("Regular");
    if (ui.samplerType->findText(sampler) < 0) {
      return;
    }

    updatingSamplerDefault = true;
    ui.samplerType->setCurrentText(sampler);
    updatingSamplerDefault = false;
  }

  void selectSamplesPerPixelDefaultForEngine(const QString& engine) {
    if (!samplesPerPixelDefaultManaged) {
      return;
    }

    const bool pathTracingSelected = engine == QStringLiteral("Path Tracer");
    const int samples = pathTracingSelected ? 64 : 1;
    const int clampedSamples =
      std::clamp(samples, ui.samplesPerPixel->minimum(), ui.samplesPerPixel->maximum());
    updatingSamplesPerPixelDefault = true;
    ui.samplesPerPixel->setValue(clampedSamples);
    updatingSamplesPerPixelDefault = false;
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
  connect(p->ui.pathTracingSchedule, SIGNAL(currentTextChanged(const QString&)), this,
          SLOT(updateEngineControls()));
  connect(p->ui.tracingExecution, SIGNAL(currentTextChanged(const QString&)), this,
          SLOT(updateEngineControls()));
  connect(p->ui.rayDenoiser, SIGNAL(currentTextChanged(const QString&)), this,
          SLOT(updateEngineControls()));
  connect(p->ui.samplerType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
    if (!p->updatingSamplerDefault) {
      p->samplerDefaultManaged = false;
    }
    emit settingsChanged();
  });
  connect(p->ui.samplesPerPixel, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
    if (!p->updatingSamplesPerPixelDefault) {
      p->samplesPerPixelDefaultManaged = false;
    }
    emit settingsChanged();
  });
  for (auto* comboBox : findChildren<QComboBox*>()) {
    if (comboBox == p->ui.samplerType) {
      continue;
    }
    connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &RenderSettingsWidget::settingsChanged);
  }
  for (auto* spinBox : findChildren<QSpinBox*>()) {
    if (spinBox == p->ui.samplesPerPixel) {
      continue;
    }
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

QString RenderSettingsWidget::pathTracingSchedule() const {
  return p->ui.pathTracingSchedule->currentText();
}

QString RenderSettingsWidget::tracingExecution() const {
  return p->ui.tracingExecution->currentText();
}

QString RenderSettingsWidget::wavefrontIntersectionBackend() const {
  return p->ui.wavefrontIntersectionBackend->currentText();
}

QString RenderSettingsWidget::wavefrontTracingBackend() const {
  return wavefrontIntersectionBackend();
}

int RenderSettingsWidget::samplesPerPixel() const {
  return p->ui.samplesPerPixel->value();
}

int RenderSettingsWidget::maxRecursionDepth() const {
  return p->ui.maxRecursionDepth->value();
}

int RenderSettingsWidget::directLightSamples() const {
  return p->ui.pathTracerDirectLightSamples->value();
}

bool RenderSettingsWidget::denoiserOverrideEnabled() const {
  return p->ui.rayDenoiser->currentText() != QStringLiteral("Scene settings");
}

QString RenderSettingsWidget::denoiser() const {
  return p->ui.rayDenoiser->currentText();
}

int RenderSettingsWidget::denoiseRadius() const {
  return p->ui.rayDenoiseRadius->value();
}

double RenderSettingsWidget::denoiseColorSigma() const {
  return p->ui.rayDenoiseColorSigma->value();
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
  p->selectSamplerDefaultForEngine(engine());
  p->selectSamplesPerPixelDefaultForEngine(engine());

  if (engine() == "Raytracer" || engine() == "Path Tracer") {
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
  const bool isRayFamily = (eng == "Raytracer" || eng == "Path Tracer");
  const bool pathTracingSelected = eng == "Path Tracer";
  const bool pathTracingUsesWavefront =
    eng == "Path Tracer" && p->ui.pathTracingSchedule->currentText() == QStringLiteral("Wavefront");
  const bool supportsPathTracingSchedule = (eng == "Path Tracer");
  const bool supportsDirectLightSamples = pathTracingSelected;
  const bool supportsTracingExecution = pathTracingSelected;
  const bool supportsRayDenoiser = pathTracingUsesWavefront;
  const bool tracingExecutionIsHybrid =
    p->ui.tracingExecution->currentText() == QStringLiteral("Hybrid");
  const bool supportsIntersectionBackend =
    eng == "Raytracer" || (pathTracingUsesWavefront && tracingExecutionIsHybrid);
  const bool denoiserIsBox = p->ui.rayDenoiser->currentText() == QStringLiteral("Box");
  const bool denoiserIsBilateral = p->ui.rayDenoiser->currentText() == QStringLiteral("Bilateral");
  const bool showRayDenoiseRadius = supportsRayDenoiser && (denoiserIsBox || denoiserIsBilateral);
  const bool showRayDenoiseColorSigma = supportsRayDenoiser && denoiserIsBilateral;
  const bool isRasterizer = (eng == "Rasterizer");
  const bool showShadowDetails = isRasterizer && shadowMapsEnabled();
  p->ui.raytracerFrame->setVisible(isRayFamily);
  p->ui.label_pathTracingSchedule->setVisible(supportsPathTracingSchedule);
  p->ui.pathTracingSchedule->setVisible(supportsPathTracingSchedule);
  p->ui.label_pathTracerDirectLightSamples->setVisible(supportsDirectLightSamples);
  p->ui.pathTracerDirectLightSamples->setVisible(supportsDirectLightSamples);
  p->ui.label_tracingExecution->setVisible(supportsTracingExecution);
  p->ui.tracingExecution->setVisible(supportsTracingExecution);
  p->ui.label_wavefrontIntersectionBackend->setVisible(supportsIntersectionBackend);
  p->ui.wavefrontIntersectionBackend->setVisible(supportsIntersectionBackend);
  p->ui.label_rayDenoiser->setVisible(supportsRayDenoiser);
  p->ui.rayDenoiser->setVisible(supportsRayDenoiser);
  p->ui.label_rayDenoiseRadius->setVisible(showRayDenoiseRadius);
  p->ui.rayDenoiseRadius->setVisible(showRayDenoiseRadius);
  p->ui.label_rayDenoiseColorSigma->setVisible(showRayDenoiseColorSigma);
  p->ui.rayDenoiseColorSigma->setVisible(showRayDenoiseColorSigma);
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

void RenderSettingsWidget::setRenderIntent(const engine::graph::RenderIntent& intent) {
  const QSignalBlocker signalBlocker(this);
  const auto& raytracerOptions = intent.engineOptions.raytracer();
  p->samplerDefaultManaged = !raytracerOptions.sampler().has_value();
  p->samplesPerPixelDefaultManaged = !raytracerOptions.samplesPerPixel().has_value();

  p->setComboBoxText(p->ui.engineType, p->engineText(intent));
  p->setComboBoxText(p->ui.pathTracingSchedule, p->pathTracingScheduleText(intent));
  p->applyRaytracerOptions(raytracerOptions);
  p->applyRasterizerOptions(intent);
  p->applyWireframeOptions(intent.engineOptions.wireframe());
  updateEngineControls();
}

void RenderSettingsWidget::setBusy(bool busy) {
  p->ui.resolution->setEnabled(!busy);
  p->ui.viewPlaneType->setEnabled(!busy);
  p->ui.samplerType->setEnabled(!busy);
  p->ui.engineType->setEnabled(!busy);
  p->ui.pathTracingSchedule->setEnabled(!busy);
  p->ui.tracingExecution->setEnabled(!busy);
  p->ui.wavefrontIntersectionBackend->setEnabled(!busy);
  p->ui.samplesPerPixel->setEnabled(!busy);
  p->ui.maxRecursionDepth->setEnabled(!busy);
  p->ui.pathTracerDirectLightSamples->setEnabled(!busy);
  p->ui.rayDenoiser->setEnabled(!busy);
  p->ui.rayDenoiseRadius->setEnabled(!busy);
  p->ui.rayDenoiseColorSigma->setEnabled(!busy);
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
