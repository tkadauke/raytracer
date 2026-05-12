#include "Display.h"

#include <QApplication>

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  auto display = new RenderDisplay;
  display->show();
  
  return app.exec();
}
