#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "render/lights/DirectionalLight.h"
#include "render/materials/MatteMaterial.h"
#include "render/materials/PhongMaterial.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/textures/ConstantColorTexture.h"

using namespace testing;
using namespace render;

namespace {
  Colord namedColor(const std::string& name) {
    if (name == "red") return Colord(1, 0, 0);
    if (name == "green") return Colord(0, 1, 0);
    if (name == "blue") return Colord(0, 0, 1);
    if (name == "white") return Colord(1, 1, 1);
    if (name == "black") return Colord(0, 0, 0);
    return Colord(1, 0, 0);
  }

  // True if any pixel in the 5×5 cluster around the buffer centre carries
  // light from a non-red channel — the visual signature of a Phong specular
  // term over a red diffuse texture.
  bool centerClusterContainsSpecularTint(const EngineFeatureTest* test) {
    const auto& buffer = test->buffer();
    const int cx = buffer.width() / 2;
    const int cy = buffer.height() / 2;
    for (int y = cy - 2; y <= cy + 2; ++y) {
      for (int x = cx - 2; x <= cx + 2; ++x) {
        const unsigned int rgb = test->colorAt(x, y);
        const unsigned int r = (rgb >> 16) & 0xff;
        const unsigned int g = (rgb >> 8) & 0xff;
        const unsigned int b = rgb & 0xff;
        if (r > 0x20 && g > 0x20 && b > 0x20) return true;
      }
    }
    return false;
  }
}

GIVEN(EngineFeatureTest, "a matte sphere with a (red|green|blue|white|black) texture") {
  auto sphere = std::make_shared<Sphere>(Vector3d::null(), 1);
  sphere->setMaterial(
    std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(namedColor(match[1]))));
  test->add(sphere);
}

GIVEN(EngineFeatureTest, "a matte sphere with ambient ([\\d.]+) and diffuse ([\\d.]+)") {
  auto sphere = std::make_shared<Sphere>(Vector3d::null(), 1);
  auto material =
    std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord(1, 0, 0)));
  material->setAmbientCoefficient(std::stod(match[1]));
  material->setDiffuseCoefficient(std::stod(match[2]));
  sphere->setMaterial(material);
  test->add(sphere);
}

GIVEN(EngineFeatureTest, "a phong sphere") {
  auto sphere = std::make_shared<Sphere>(Vector3d::null(), 1);
  sphere->setMaterial(
    std::make_shared<PhongMaterial>(std::make_shared<ConstantColorTexture>(Colord(1, 0, 0))));
  test->add(sphere);
}

// Red diffuse + white specular at full strength, with ambient zeroed so the
// specular contribution shows up cleanly against a black scene.
GIVEN(EngineFeatureTest, "a phong sphere with white specular") {
  auto sphere = std::make_shared<Sphere>(Vector3d::null(), 1);
  auto material =
    std::make_shared<PhongMaterial>(std::make_shared<ConstantColorTexture>(Colord(1, 0, 0)));
  material->setAmbientCoefficient(0);
  material->setDiffuseCoefficient(1);
  material->setSpecularColor(Colord::white());
  material->setSpecularCoefficient(1);
  sphere->setMaterial(material);
  test->add(sphere);
}

// Black ambient + black background isolates lit-pixel contributions from the
// scene's default white backdrop, so the specular highlight test can read the
// material output directly.
GIVEN(EngineFeatureTest, "a dark scene") {
  test->scene()->setAmbient(Colord::black());
  test->scene()->setBackground(Colord::black());
}

GIVEN(EngineFeatureTest,
      "a directional light from \\(([\\-\\d.]+), ([\\-\\d.]+), ([\\-\\d.]+)\\)") {
  test->scene()->addLight(std::make_shared<DirectionalLight>(
    Vector3d(std::stod(match[1]), std::stod(match[2]), std::stod(match[3])), Colord(1, 1, 1)));
}

THEN(EngineFeatureTest, "i should see a dim red sphere") {
  ASSERT_TRUE(test->colorPresent(Colord(0.5, 0, 0)));
  ASSERT_FALSE(test->colorPresent(Colord(1, 0, 0)));
}

THEN(EngineFeatureTest, "i should see a (red|green|blue) sphere") {
  ASSERT_TRUE(test->colorPresent(namedColor(match[1])));
  if (match[1] != "red") ASSERT_FALSE(test->colorPresent(Colord(1, 0, 0)));
}

THEN(EngineFeatureTest, "i should see a specular highlight") {
  ASSERT_TRUE(centerClusterContainsSpecularTint(test));
}

THEN(EngineFeatureTest, "i should not see a specular highlight") {
  ASSERT_FALSE(centerClusterContainsSpecularTint(test));
}
