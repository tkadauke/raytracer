#include "test/functional/support/EngineFeatureTest.h"

#include "render/RenderEngine.h"
#include "render/cameras/PinholeCamera.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Scene.h"
#include "render/textures/ConstantColorTexture.h"

#include "test/helpers/ImageViewer.h"

namespace testing {
  using namespace render;

  EngineFeatureTest::EngineFeatureTest()
      : FeatureTest<EngineFeatureTest>(),
        m_camera(0),
        m_engine(0),
        m_buffer(200, 150),
        m_cancelled(false) {
    previousObjectSize = 0;
    previousEdgeCount = 0;
  }

  void EngineFeatureTest::beforeThen() {
    render();
  }

  void EngineFeatureTest::SetUp() {
    m_scene = std::make_shared<Scene>(Colord(1, 1, 1));
  }

  void EngineFeatureTest::TearDown() {
    m_scene.reset();
  }

  void EngineFeatureTest::add(std::shared_ptr<Primitive> primitive) {
    m_scene->add(primitive);
  }

  Scene* EngineFeatureTest::scene() const {
    return m_scene.get();
  }

  std::shared_ptr<render::Camera> EngineFeatureTest::camera() {
    if (!m_camera)
      m_camera = std::make_shared<PinholeCamera>();
    return m_camera;
  }

  void EngineFeatureTest::setCamera(const Vector3d& position, const Vector3d& lookAt) {
    setCamera(std::make_shared<PinholeCamera>(position, lookAt));
  }

  void EngineFeatureTest::setCamera(std::shared_ptr<render::Camera> camera) {
    m_camera = camera;
  }

  void EngineFeatureTest::setView(const Vector3d& position, const Vector3d& lookAt) {
    camera()->setPosition(position);
    camera()->setTarget(lookAt);
  }

  void EngineFeatureTest::render() {
    m_engine = createEngine();
    if (m_cancelled) {
      m_engine->cancel();
    }
    m_engine->render(m_buffer);
  }

  void EngineFeatureTest::cancel() {
    m_cancelled = true;
    if (m_engine) {
      m_engine->cancel();
    }
  }

  void EngineFeatureTest::uncancel() {
    m_cancelled = false;
    if (m_engine) {
      m_engine->uncancel();
    }
  }

  void EngineFeatureTest::clear() {
    m_buffer.clear();
  }

  const Buffer<unsigned int>& EngineFeatureTest::buffer() const {
    return m_buffer;
  }

  bool EngineFeatureTest::colorPresent(const Colord& color) const {
    return colorCount(color) > 0;
  }

  int EngineFeatureTest::colorCount(const Colord& color) const {
    int result = 0;
    for (int i = 0; i != m_buffer.width(); ++i) {
      for (int j = 0; j != m_buffer.height(); ++j) {
        if (m_buffer[j][i] == color.rgb()) {
          result++;
        }
      }
    }
    return result;
  }

  unsigned int EngineFeatureTest::colorAt(int x, int y) const {
    return m_buffer[y][x];
  }

  void EngineFeatureTest::show() const {
    ImageViewer viewer(m_buffer);
    viewer.show();
  }

  std::shared_ptr<Material> EngineFeatureTest::redDiffuse() const {
    return std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord(1, 0, 0)));
  }

  void EngineFeatureTest::lookAtOrigin() {
    setView(Vector3d(0, 0, -5), Vector3d::null);
  }

  void EngineFeatureTest::lookAway() {
    setView(Vector3d(0, 0, -20), Vector3d(0, 0, -25));
  }

  void EngineFeatureTest::goFarAway() {
    setView(Vector3d(0, 0, -30), Vector3d::null);
  }

  Colord EngineFeatureTest::primaryColor() const {
    return Colord(1, 0, 0);
  }

  bool EngineFeatureTest::objectVisible() const {
    return colorPresent(primaryColor());
  }

  int EngineFeatureTest::objectSize() const {
    return colorCount(primaryColor());
  }
}
