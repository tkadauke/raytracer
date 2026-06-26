#pragma once
#include "widgets/RenderWidget.h"

#include <memory>

#include <QWidget>

namespace engine::graph {
  struct RenderIntent;
}

class RenderSettingsWidget : public QWidget {
  Q_OBJECT

public:
  explicit RenderSettingsWidget(QWidget* parent = nullptr);
  ~RenderSettingsWidget();

  QSize resolution() const;
  QString sampler() const;
  QString sampleStreamMode() const;
  QString viewPlane() const;
  QString engine() const;
  QString pathTracingSchedule() const;
  QString wavefrontTracingBackend() const;
  QString tracingExecution() const;
  QString wavefrontIntersectionBackend() const;
  int samplesPerPixel() const;
  int maxRecursionDepth() const;
  int directLightSamples() const;
  bool denoiserOverrideEnabled() const;
  QString denoiser() const;
  int denoiseRadius() const;
  double denoiseColorSigma() const;
  int renderThreads() const;
  int queueSize() const;
  int lod() const;
  QString rasterBackend() const;
  int msaaSamples() const;
  QString msaaShadingMode() const;
  QString postProcessAA() const;
  bool shadowMapsEnabled() const;
  int shadowMapSize() const;
  int shadowCascadeCount() const;
  double shadowCascadeSplitLambda() const;
  double shadowBias() const;
  double shadowSlopeBias() const;
  int shadowFilterRadius() const;
  QString shadowFilterMode() const;
  RenderWidget::DisplayMode displayMode() const;

  bool showProgressIndicators() const;

  void setRenderIntent(const engine::graph::RenderIntent& intent);
  void setBusy(bool busy);
  void setElapsedTime(int milliseconds);

signals:
  void renderClicked();
  void stopClicked();
  void settingsChanged();

private slots:
  void render();
  void stop();
  void engineChanged();
  void updateEngineControls();

private:
  struct Private;
  std::unique_ptr<Private> p;
};
