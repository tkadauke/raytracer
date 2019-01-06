#pragma once

#include <QWidget>

class RenderSettingsWidget : public QWidget {
  Q_OBJECT;

public:
  explicit RenderSettingsWidget(QWidget* parent = nullptr);
  ~RenderSettingsWidget();

  QSize resolution() const;
  QString sampler() const;
  QString viewPlane() const;
  int samplesPerPixel() const;
  int maxRecursionDepth() const;
  int renderThreads() const;
  int queueSize() const;

  bool showProgressIndicators() const;

  void setBusy(bool busy);
  void setElapsedTime(int milliseconds);

signals:
  void renderClicked();
  void stopClicked();

private slots:
  void render();
  void stop();

private:
  struct Private;
  std::unique_ptr<Private> p;
};
