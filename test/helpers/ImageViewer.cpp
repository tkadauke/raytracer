#include "test/helpers/ImageViewer.h"
#include <QApplication>
#include <QPainter>

ImageViewerWidget::ImageViewerWidget(const Buffer& buffer)
  : m_buffer(buffer)
{
  resize(m_buffer.width(), m_buffer.height());
}

void ImageViewerWidget::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  QImage image(width(), height(), QImage::Format_RGB32);
  
  for (int i = 0; i != width(); ++i) {
    for (int j = 0; j != height(); ++j) {
      image.setPixel(i, j, m_buffer[j][i].rgb());
    }
  }
  
  painter.drawImage(QPoint(0, 0), image);
}

ImageViewer::ImageViewer(const Buffer& buffer) {
  int argc = 0;
  char** argv = 0;
  m_application = new QApplication(argc, argv);
  
  m_widget = new ImageViewerWidget(buffer);
}

void ImageViewer::show() {
  m_widget->show();
  m_application->exec();
}
