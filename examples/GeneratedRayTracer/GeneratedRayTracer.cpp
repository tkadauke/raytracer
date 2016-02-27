#include "MainWindow.h"
#include "core/math/Vector.h"
#include "core/math/Angle.h"
#include "core/Color.h"

#include "world/objects/Material.h"
#include "world/objects/Texture.h"

#include <QApplication>

Q_DECLARE_METATYPE(Vector3d);
Q_DECLARE_METATYPE(Angled);
Q_DECLARE_METATYPE(Colord);

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  
  qRegisterMetaType<Vector3d>();
  qRegisterMetaType<Angled>();
  qRegisterMetaType<Colord>();
  qRegisterMetaType<Material*>();
  qRegisterMetaType<Texture*>();
  
  auto window = new MainWindow;
  window->resize(1280, 768);
  window->show();
  
  return app.exec();
}
