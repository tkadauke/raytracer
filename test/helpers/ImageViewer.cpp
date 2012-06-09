#include "test/helpers/ImageViewer.h"
#include <QApplication>
#include <QPainter>

ImageViewerWidget::ImageViewerWidget(const Buffer<unsigned int>& buffer)
  : m_buffer(buffer)
{
  resize(m_buffer.width(), m_buffer.height());
}

void ImageViewerWidget::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  QImage image(m_buffer.width(), m_buffer.height(), QImage::Format_RGB32);
  
  for (int i = 0; i != m_buffer.width(); ++i) {
    for (int j = 0; j != m_buffer.height(); ++j) {
      image.setPixel(i, j, m_buffer[j][i]);
    }
  }
  
  painter.drawImage(QPoint(0, 0), image);
}

ImageViewer::ImageViewer(const Buffer<unsigned int>& buffer) {
  int argc = 0;
  char** argv = 0;
  m_application = new QApplication(argc, argv);
  
  m_widget = new ImageViewerWidget(buffer);
}

void ImageViewer::show() {
  m_widget->show();
  m_application->exec();
}
