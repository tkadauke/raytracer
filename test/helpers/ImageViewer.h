#ifndef IMAGE_VIEWER_H
#define IMAGE_VIEWER_H

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
