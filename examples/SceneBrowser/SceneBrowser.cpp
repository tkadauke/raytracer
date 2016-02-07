#include "Display.h"

#include <QApplication>

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  auto display = new Display;
  display->show();
  
  return app.exec();
}
