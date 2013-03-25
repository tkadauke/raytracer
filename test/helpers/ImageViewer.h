#ifndef IMAGE_VIEWER_H
#define IMAGE_VIEWER_H

#include <QWidget>

#include "core/Buffer.h"

class QApplication;

class ImageViewerWidget : public QWidget {
public:
  ImageViewerWidget(const Buffer<unsigned int>& buffer);
  
  virtual void paintEvent(QPaintEvent*);

private:
  const Buffer<unsigned int>& m_buffer;
};

class ImageViewer {
public:
  ImageViewer(const Buffer<unsigned int>& buffer);
  
  void show();
  
private:
  QApplication* m_application;
  ImageViewerWidget* m_widget;
};

#endif
