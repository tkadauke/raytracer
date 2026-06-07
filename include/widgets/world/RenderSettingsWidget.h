#pragma once
#include "widgets/RenderWidget.h"

#include <memory>

#include <QWidget>

class RenderSettingsWidget : public QWidget {
  Q_OBJECT

public:
  explicit RenderSettingsWidget(QWidget* parent = nullptr);
  ~RenderSettingsWidget();

  QSize resolution() const;
  QString sampler() const;
  QString viewPlane() const;
  QString engine() const;
  int samplesPerPixel() const;
  int maxRecursionDepth() const;
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
