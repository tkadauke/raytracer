#include "MainWindow.h"
#include "core/math/Vector.h"
#include "core/Color.h"

#include "world/objects/Material.h"

#include <QApplication>

Q_DECLARE_METATYPE(Vector3d);
Q_DECLARE_METATYPE(Colord);

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  qRegisterMetaType<Vector3d>();
  qRegisterMetaType<Colord>();
  qRegisterMetaType<Material*>();
  
  auto window = new MainWindow;
  window->resize(1280, 768);
  window->show();
  
  return app.exec();
}
