#include "test/helpers/RaytracerTestHelper.h"
#include "test/helpers/ImageViewer.h"

#include "surfaces/Scene.h"
#include "Raytracer.h"
#include "cameras/PinholeCamera.h"
#include "surfaces/Sphere.h"
#include "materials/MatteMaterial.h"

namespace testing {
  RaytracerFunctionalTest::RaytracerFunctionalTest()
    : ::testing::Test(), m_camera(0), m_raytracer(0), m_buffer(200, 150)
  {
  }
  
  void RaytracerFunctionalTest::SetUp() {
    m_scene = new Scene(Colord(1, 1, 1));
  }
  
  void RaytracerFunctionalTest::TearDown() {
    delete m_scene;
    if (m_raytracer)
      delete m_raytracer;
    if (m_camera)
      delete m_camera;
  }

  void RaytracerFunctionalTest::add(Surface* surface) {
    m_scene->add(surface);
  }
  
  Camera* RaytracerFunctionalTest::camera() {
    if (!m_camera)
      m_camera = new PinholeCamera;
    return m_camera;
  }

  void RaytracerFunctionalTest::setCamera(const Vector3d& position, const Vector3d& lookAt) {
    setCamera(new PinholeCamera(position, lookAt));
  }

  void RaytracerFunctionalTest::setCamera(Camera* camera) {
    if (m_camera)
      delete m_camera;
    m_camera = camera;
  }

  void RaytracerFunctionalTest::setView(const Vector3d& position, const Vector3d& lookAt) {
    camera()->setPosition(position);
    camera()->setTarget(lookAt);
  }

  void RaytracerFunctionalTest::render() {
    m_raytracer = new Raytracer(m_camera, m_scene);
    m_raytracer->render(m_buffer);
  }
  
  bool RaytracerFunctionalTest::colorPresent(const Colord& color) {
    return colorCount(color) > 0;
  }
  
  int RaytracerFunctionalTest::colorCount(const Colord& color) {
    int result = 0;
    for (int i = 0; i != m_buffer.width(); ++i) {
      for (int j = 0; j != m_buffer.height(); ++j) {
        if (m_buffer[j][i] == color) {
          result++;
        }
      }
    }
    return result;
  }
  
  void RaytracerFunctionalTest::show() {
    ImageViewer viewer(m_buffer);
    viewer.show();
  }
  
  Material* RaytracerFunctionalTest::redDiffuse() {
    Material* material = new MatteMaterial(Colord(1, 0, 0));
    return material;
  }
  
  Sphere* RaytracerFunctionalTest::centeredSphere() {
    Sphere* sphere = new Sphere(Vector3d::null, 1);
    sphere->setMaterial(redDiffuse());
    return sphere;
  }
  
  Sphere* RaytracerFunctionalTest::displacedSphere() {
    Sphere* sphere = new Sphere(Vector3d(0, 20, 0), 1);
    sphere->setMaterial(redDiffuse());
    return sphere;
  }
  
  void RaytracerFunctionalTest::lookAtOrigin() {
    setView(Vector3d(0, 0, -5), Vector3d::null);
  }
  
  void RaytracerFunctionalTest::lookAway() {
    setView(Vector3d(0, 0, -20), Vector3d(0, 0, -25));
  }
  
  void RaytracerFunctionalTest::goFarAway() {
    setView(Vector3d(0, 0, -30), Vector3d::null);
  }
  
  bool RaytracerFunctionalTest::objectVisible() {
    return colorPresent(Colord(1, 0, 0));
  }
  
  int RaytracerFunctionalTest::objectSize() {
    return colorCount(Colord(1, 0, 0));
  }
}
