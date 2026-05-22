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
  int renderThreads() const;
  int queueSize() const;
  int lod() const;
  int msaaSamples() const;
  QString postProcessAA() const;
  bool shadowMapsEnabled() const;
  int shadowMapSize() const;
  double shadowBias() const;
  int shadowFilterRadius() const;
  QString shadowFilterMode() const;
  RenderWidget::DisplayMode displayMode() const;

  bool showProgressIndicators() const;

  void setBusy(bool busy);
  void setElapsedTime(int milliseconds);

signals:
  void renderClicked();
  void stopClicked();

private slots:
  void render();
  void stop();
  void engineChanged();
  void updateEngineControls();

private:
  struct Private;
  std::unique_ptr<Private> p;
};
