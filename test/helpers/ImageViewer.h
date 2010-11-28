#ifndef IMAGE_VIEWER
#define IMAGE_VIEWER

#include <QWidget>

#include "Buffer.h"

class QApplication;

class ImageViewerWidget : public QWidget {
public:
  ImageViewerWidget(const Buffer& buffer);
  
  virtual void paintEvent(QPaintEvent*);

private:
  const Buffer& m_buffer;
};

class ImageViewer {
public:
  ImageViewer(const Buffer& buffer);
  
  void show();
  
private:
  QApplication* m_application;
  ImageViewerWidget* m_widget;
};

#endif
