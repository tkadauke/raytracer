#ifndef RAYTRACER_TEST_HELPER
#define RAYTRACER_TEST_HELPER

#include "gtest/gtest.h"
#include "Vector.h"
#include "Buffer.h"
#include "Color.h"

class Surface;
class Scene;
class Raytracer;

namespace testing {
  class RaytracerFunctionalTest : public ::testing::Test {
  public:
    RaytracerFunctionalTest();
    
  protected:
    virtual void SetUp();
    virtual void TearDown();
    
    void add(Surface* surface);
    void setCamera(const Vector3d& position, const Vector3d& lookAt);
    void render();
    
    bool colorPresent(const Colord& color);
    void show();
  
  private:
    Scene* m_scene;
    Raytracer* m_raytracer;
    Buffer m_buffer;
    Vector3d m_position, m_lookAt;
  };
}

#endif
