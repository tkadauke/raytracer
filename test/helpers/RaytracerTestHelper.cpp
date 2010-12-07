#include "test/helpers/RaytracerTestHelper.h"
#include "test/helpers/ImageViewer.h"

#include "Scene.h"
#include "Raytracer.h"
#include "PinholeCamera.h"

namespace testing {
  RaytracerFunctionalTest::RaytracerFunctionalTest()
    : ::testing::Test(), m_raytracer(0), m_buffer(200, 150)
  {
  }
  
  void RaytracerFunctionalTest::SetUp() {
    m_scene = new Scene(Colord(0.9, 0.9, 0.9));
  }
  
  void RaytracerFunctionalTest::TearDown() {
    delete m_scene;
    if (m_raytracer)
      delete m_raytracer;
  }

  void RaytracerFunctionalTest::add(Surface* surface) {
    m_scene->add(surface);
  }

  void RaytracerFunctionalTest::setCamera(const Vector3d& position, const Vector3d& lookAt) {
    m_position = position;
    m_lookAt = lookAt;
  }

  void RaytracerFunctionalTest::render() {
    PinholeCamera camera(m_position, m_lookAt);
    m_raytracer = new Raytracer(&camera, m_scene);
    m_raytracer->render(m_buffer);
  }
  
  bool RaytracerFunctionalTest::colorPresent(const Colord& color) {
    for (int i = 0; i != m_buffer.width(); ++i) {
      for (int j = 0; j != m_buffer.height(); ++j) {
        if (m_buffer[j][i] == color)
          return true;
      }
    }
    return false;
  }
  
  void RaytracerFunctionalTest::show() {
    ImageViewer viewer(m_buffer);
    viewer.show();
  }
}
