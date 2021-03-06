#include <QVBoxLayout>
#include <QSpacerItem>

#include <QMouseEvent>

#include "Display.h"
#include "raytracer/Raytracer.h"
#include "raytracer/State.h"
#include "raytracer/cameras/CameraFactory.h"
#include "raytracer/cameras/PinholeCamera.h"
#include "SceneFactory.h"
#include "SceneWidget.h"
#include "widgets/ViewPlaneTypeWidget.h"
#include "widgets/CameraTypeWidget.h"
#include "widgets/CameraParameterWidgetFactory.h"
#include "raytracer/viewplanes/ViewPlaneFactory.h"
#include "core/math/HitPointInterval.h"

using namespace std;
using namespace raytracer;

Display::Display()
  : QtDisplay(nullptr, std::make_shared<Raytracer>(nullptr)),
    m_camera(std::make_shared<PinholeCamera>()),
    m_cameraParameter(nullptr)
{
  m_sidebar = new QWidget(nullptr, Qt::Drawer);
  m_sidebar->show();
  m_verticalLayout = new QVBoxLayout(m_sidebar);
  m_verticalLayout->setContentsMargins(6, 6, 6, 6);
  
  m_scene = new SceneWidget(m_sidebar);
  m_verticalLayout->addWidget(m_scene);

  m_viewPlaneType = new ViewPlaneTypeWidget(m_sidebar);
  m_verticalLayout->addWidget(m_viewPlaneType);
  
  m_cameraType = new CameraTypeWidget(m_sidebar);
  m_verticalLayout->addWidget(m_cameraType);
  
  auto verticalSpacer = new QSpacerItem(20, 186, QSizePolicy::Minimum, QSizePolicy::Expanding);
  m_verticalLayout->addItem(verticalSpacer);
  
  m_raytracer->setCamera(m_camera);
  m_raytracer->setScene(SceneFactory::self().create("Glass Boxes"));
  m_camera->setViewPlane(ViewPlaneFactory::self().createShared(m_viewPlaneType->type()));
  connect(m_scene, SIGNAL(changed()), this, SLOT(sceneChanged()));
  connect(m_viewPlaneType, SIGNAL(changed()), this, SLOT(viewPlaneTypeChanged()));
  connect(m_cameraType, SIGNAL(changed()), this, SLOT(cameraTypeChanged()));
}

Display::~Display() {
  delete m_sidebar;
}

void Display::sceneChanged() {
  stop();
  m_raytracer->setScene(SceneFactory::self().create(m_scene->sceneName()));
  render();
}

void Display::viewPlaneTypeChanged() {
  m_camera->setViewPlane(ViewPlaneFactory::self().createShared(m_viewPlaneType->type()));
  render();
}

void Display::cameraTypeChanged() {
  stop();
  m_camera = std::shared_ptr<Camera>(CameraFactory::self().create(m_cameraType->type()));
  m_raytracer->setCamera(m_camera);
  
  if (m_cameraParameter) {
    delete m_cameraParameter;
  }

  m_cameraParameter = CameraParameterWidgetFactory::self().create(m_cameraType->type());
  if (m_cameraParameter) {
    m_verticalLayout->addWidget(m_cameraParameter);
    connect(m_cameraParameter, SIGNAL(changed()), this, SLOT(cameraParameterChanged()));
  }
  
  viewPlaneTypeChanged();
}

void Display::cameraParameterChanged() {
  stop();
  m_cameraParameter->applyTo(m_camera);
  render();
}

void Display::mousePressEvent(QMouseEvent* event) {
  QtDisplay::mousePressEvent(event);
  
  Rayd ray = m_camera->rayForPixel(event->pos().x(), event->pos().y());
  if (ray.direction().isDefined()) {
    auto state = m_raytracer->rayState(ray);
  
    cout << state.hitPoint.primitive() << " - " << state.hitPoint << endl;
  }
}

#include "Display.moc"
