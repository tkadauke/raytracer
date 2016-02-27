#pragma once

#include <QWidget>

class RenderSettingsWidget : public QWidget {
  Q_OBJECT
  
public:
  RenderSettingsWidget(QWidget* parent = nullptr);
  ~RenderSettingsWidget();
  
  QSize resolution();
  int samplesPerPixel();
  int maxRecursionDepth();
  
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
