#include "test/functional/support/RaytracerFeatureTest.h"

#include "surfaces/Scene.h"
#include "Raytracer.h"
#include "cameras/PinholeCamera.h"
#include "materials/MatteMaterial.h"

#include "test/helpers/ImageViewer.h"

namespace testing {
  RaytracerFeatureTest::RaytracerFeatureTest()
    : FeatureTest<RaytracerFeatureTest>(), m_camera(0), m_raytracer(0), m_buffer(200, 150)
  {
    previousObjectSize = 0;
  }
  
  void RaytracerFeatureTest::beforeThen() {
    render();
  }

  void RaytracerFeatureTest::SetUp() {
    m_scene = new Scene(Colord(1, 1, 1));
  }

  void RaytracerFeatureTest::TearDown() {
    delete m_scene;
    if (m_raytracer)
      delete m_raytracer;
    if (m_camera)
      delete m_camera;
  }

  void RaytracerFeatureTest::add(Surface* surface) {
    m_scene->add(surface);
  }

  Camera* RaytracerFeatureTest::camera() {
    if (!m_camera)
      m_camera = new PinholeCamera;
    return m_camera;
  }

  void RaytracerFeatureTest::setCamera(const Vector3d& position, const Vector3d& lookAt) {
    setCamera(new PinholeCamera(position, lookAt));
  }

  void RaytracerFeatureTest::setCamera(Camera* camera) {
    if (m_camera)
      delete m_camera;
    m_camera = camera;
  }

  void RaytracerFeatureTest::setView(const Vector3d& position, const Vector3d& lookAt) {
    camera()->setPosition(position);
    camera()->setTarget(lookAt);
  }

  void RaytracerFeatureTest::render() {
    m_raytracer = new Raytracer(m_camera, m_scene);
    m_raytracer->render(m_buffer);
  }

  bool RaytracerFeatureTest::colorPresent(const Colord& color) {
    return colorCount(color) > 0;
  }

  int RaytracerFeatureTest::colorCount(const Colord& color) {
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

  void RaytracerFeatureTest::show() {
    ImageViewer viewer(m_buffer);
    viewer.show();
  }

  Material* RaytracerFeatureTest::redDiffuse() {
    Material* material = new MatteMaterial(Colord(1, 0, 0));
    return material;
  }

  void RaytracerFeatureTest::lookAtOrigin() {
    setView(Vector3d(0, 0, -5), Vector3d::null);
  }

  void RaytracerFeatureTest::lookAway() {
    setView(Vector3d(0, 0, -20), Vector3d(0, 0, -25));
  }

  void RaytracerFeatureTest::goFarAway() {
    setView(Vector3d(0, 0, -30), Vector3d::null);
  }

  bool RaytracerFeatureTest::objectVisible() {
    return colorPresent(Colord(1, 0, 0));
  }

  int RaytracerFeatureTest::objectSize() {
    return colorCount(Colord(1, 0, 0));
  }
}