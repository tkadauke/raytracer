#include "MainWindow.h"
#include "core/math/Vector.h"

#include <QApplication>

Q_DECLARE_METATYPE(Vector3d)

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  qRegisterMetaType<Vector3d>();
  
  auto window = new MainWindow;
  window->resize(1280, 768);
  window->show();
  
  return app.exec();
}
