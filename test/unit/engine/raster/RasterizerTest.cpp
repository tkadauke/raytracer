#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "core/geometry/Mesh.h"
#include "core/math/HitPoint.h"
#include "engine/raster/Rasterizer.h"
#include "engine/raster/RasterVisibilitySet.h"
#include "engine/raster/detail/RasterMaterial.h"
#include "engine/raster/detail/RasterMaterialEvaluator.h"
#include "engine/raster/detail/RasterMSAA.h"
#include "engine/raster/detail/RasterPipelineTypes.h"
#include "engine/raster/detail/RasterShadowMapBuilder.h"
#include "engine/raster/detail/RasterShadowMaps.h"
#include "render/cameras/OrthographicCamera.h"
#include "render/cameras/PinholeCamera.h"
#include "render/cameras/ThinLensCamera.h"
#include "render/lights/DirectionalLight.h"
#include "render/materials/MatteMaterial.h"
#include "render/materials/PhongMaterial.h"
#include "render/materials/ReflectiveMaterial.h"
#include "render/materials/TransparentMaterial.h"
#include "render/primitives/Box.h"
#include "render/primitives/Curve.h"
#include "render/primitives/Instance.h"
#include "render/primitives/MeshPrimitive.h"
#include "render/primitives/Rectangle.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/primitives/Triangle.h"
#include "render/textures/CheckerBoardTexture.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/textures/ImageTexture.h"
#include "render/textures/TintedTexture.h"
#include "render/textures/Texture.h"
#include "render/textures/UVColorTexture.h"
#include "render/textures/mappings/PlanarMapping2D.h"
#include "render/textures/mappings/UVMapping2D.h"
#include "test/helpers/BufferTestHelper.h"
#include "test/helpers/CameraTestHelper.h"
#include "test/helpers/ColorTestHelper.h"
#include "test/helpers/MaterialTestHelper.h"
#include "test/helpers/SceneTestHelper.h"

#include <QThreadPool>

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace RasterizerTest {
  using namespace ::testing;
  using namespace render;
  using namespace engine::raster;
  using test::helpers::angledCamera;
  using test::helpers::countFiniteDepths;
  using test::helpers::countPixels;
  using test::helpers::countPixelsNotEqualTo;
  using test::helpers::countPixelsBrightenedByFiltering;
  using test::helpers::countPixelsDarkenedByFiltering;
  using test::helpers::matte;
  using test::helpers::standardCamera;

  class HitPointUVTexture : public Texturec {
  public:
    virtual Colord evaluate(const Rayd&, const HitPoint& hitPoint) const {
      return Colord(hitPoint.uv().x(), hitPoint.uv().y(), 0.0);
    }
  };

  class OverridingUVColorTexture : public render::UVColorTexture {
  public:
    Colord evaluate(const Rayd&, const HitPoint& hitPoint) const override {
      return Colord(0.0, hitPoint.uv().x(), hitPoint.uv().y());
    }
  };

  class OverridingConstantColorTexture : public ConstantColorTexture {
  public:
    OverridingConstantColorTexture()
        : ConstantColorTexture(Colord::red()) {
    }

    Colord evaluate(const Rayd&, const HitPoint& hitPoint) const override {
      return Colord(hitPoint.uv().x(), hitPoint.uv().y(), 0.0);
    }
  };

  class CountingPrimitive : public Primitive {
  public:
    CountingPrimitive(const BoundingBoxd& bounds, int* tessellateCalls)
        : m_bounds(bounds),
          m_tessellateCalls(tessellateCalls) {
    }

    const Primitive* intersect(const Rayd&, HitPointInterval&, render::State&) const override {
      return nullptr;
    }

    std::shared_ptr<Mesh> tessellate(int = 0) const override {
      ++(*m_tessellateCalls);
      return std::make_shared<Mesh>();
    }

  protected:
    BoundingBoxd calculateBoundingBox() const override {
      return m_bounds;
    }

  private:
    BoundingBoxd m_bounds;
    int* m_tessellateCalls;
  };

  class RecordingPrimitive : public Primitive {
  public:
    RecordingPrimitive(int id, std::vector<int>* tessellationOrder)
        : m_id(id),
          m_tessellationOrder(tessellationOrder) {
    }

    const Primitive* intersect(const Rayd&, HitPointInterval&, render::State&) const override {
      return nullptr;
    }

    std::shared_ptr<Mesh> tessellate(int = 0) const override {
      m_tessellationOrder->push_back(m_id);
      return std::make_shared<Mesh>();
    }

  protected:
    BoundingBoxd calculateBoundingBox() const override {
      return BoundingBoxd(Vector3d(-1.0, -1.0, 0.0), Vector3d(1.0, 1.0, 1.0));
    }

  private:
    int m_id;
    std::vector<int>* m_tessellationOrder;
  };

  class LodRecordingPrimitive : public Primitive {
  public:
    explicit LodRecordingPrimitive(std::vector<int>* lods)
        : m_lods(lods) {
    }

    const Primitive* intersect(const Rayd&, HitPointInterval&, render::State&) const override {
      return nullptr;
    }

    std::shared_ptr<Mesh> tessellate(int lod = 0) const override {
      m_lods->push_back(lod);
      Mesh mesh;
      mesh.addVertex(Vector3d(-1.0, -1.0, 0.0), Vector3d(0.0, 0.0, -1.0));
      mesh.addVertex(Vector3d(1.0, -1.0, 0.0), Vector3d(0.0, 0.0, -1.0));
      mesh.addVertex(Vector3d(0.0, 1.0, 0.0), Vector3d(0.0, 0.0, -1.0));
      mesh.addFace({0, 1, 2});
      return std::make_shared<Mesh>(std::move(mesh));
    }

  protected:
    BoundingBoxd calculateBoundingBox() const override {
      return BoundingBoxd(Vector3d(-1.0, -1.0, 0.0), Vector3d(1.0, 1.0, 0.0));
    }

  private:
    std::vector<int>* m_lods;
  };

  class CountingComposite : public Composite {
  public:
    explicit CountingComposite(int* flattenCalls)
        : m_flattenCalls(flattenCalls) {
    }

    void forEachLeaf(std::shared_ptr<render::Material> inheritedMaterial,
                     const LeafVisitor& visitor) const override {
      ++(*m_flattenCalls);
      Composite::forEachLeaf(inheritedMaterial, visitor);
    }

    void forEachTransformedLeaf(std::shared_ptr<render::Material> inheritedMaterial,
                                const Matrix4d& pointMatrix, const Matrix3d& normalMatrix,
                                const TransformedLeafVisitor& visitor) const override {
      ++(*m_flattenCalls);
      Composite::forEachTransformedLeaf(inheritedMaterial, pointMatrix, normalMatrix, visitor);
    }

  private:
    int* m_flattenCalls;
  };

  class NonSpatialScene : public Scene {
  public:
    void forEachLeafInBounds(const BoundsFilter&,
                             std::shared_ptr<render::Material> inheritedMaterial,
                             const LeafVisitor& visitor) const override {
      Composite::forEachLeaf(inheritedMaterial, visitor);
    }

    void forEachTransformedLeafInBounds(const BoundsFilter& boundsFilter,
                                        std::shared_ptr<render::Material> inheritedMaterial,
                                        const Matrix4d& pointMatrix, const Matrix3d& normalMatrix,
                                        const TransformedLeafVisitor& visitor) const override {
      Composite::forEachTransformedLeaf(inheritedMaterial, pointMatrix, normalMatrix,
                                        [&](const TransformedLeaf& leaf) {
                                          if (boundsFilter(leaf.boundingBox())) {
                                            visitor(leaf);
                                          }
                                        });
    }
  };

  struct TrackedTriangleScene {
    std::shared_ptr<Scene> scene;
    std::shared_ptr<Triangle> triangle;
    std::shared_ptr<MatteMaterial> material;
  };

  static void expectBuffersEqual(const Buffer<Colord>& expected, const Buffer<Colord>& actual) {
    ASSERT_EQ(expected.width(), actual.width());
    ASSERT_EQ(expected.height(), actual.height());
    for (int y = 0; y < expected.height(); ++y)
      for (int x = 0; x < expected.width(); ++x)
        EXPECT_EQ(expected[y][x], actual[y][x]) << "at (" << x << ", " << y << ")";
  }


  static std::shared_ptr<Scene> sceneWithBox() {
    return test::helpers::sceneWithBox(Colord::white());
  }

  static std::shared_ptr<Scene> sceneWithSphere() {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(std::make_shared<Sphere>(Vector3d::null, 1));
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithCurveTube() {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(
      std::make_shared<Curve>(core::Polyline({Vector3d(-1.0, -0.5, 0.0), Vector3d(0.0, 0.5, 0.0),
                                              Vector3d(1.0, -0.5, 0.0)}),
                              0.35, Curve::TessellationMode::Tube));
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithOversizedRectangle() {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(std::make_shared<Rectangle>(Vector3d(-100, -100, 0), Vector3d(200, 0, 0),
                                           Vector3d(0, 200, 0)));
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithTriangleGrid(int columns, int rows) {
    auto scene = std::make_shared<Scene>(Colord::black());
    const double viewWidth = 8.0;
    const double viewHeight = 6.0;
    const double cellWidth = viewWidth / static_cast<double>(columns);
    const double cellHeight = viewHeight / static_cast<double>(rows);
    const double left = -viewWidth / 2.0;
    const double top = -viewHeight / 2.0;

    for (int y = 0; y != rows; ++y) {
      for (int x = 0; x != columns; ++x) {
        const double x0 = left + static_cast<double>(x) * cellWidth;
        const double x1 = x0 + cellWidth;
        const double y0 = top + static_cast<double>(y) * cellHeight;
        const double y1 = y0 + cellHeight;
        scene->add(std::make_shared<Triangle>(Vector3d(x0, y0, 0.0), Vector3d(x1, y0, 0.0),
                                              Vector3d(x0, y1, 0.0)));
        scene->add(std::make_shared<Triangle>(Vector3d(x1, y0, 0.0), Vector3d(x1, y1, 0.0),
                                              Vector3d(x0, y1, 0.0)));
      }
    }

    return scene;
  }

  static std::shared_ptr<Scene> sceneWithLargeScreenTriangles() {
    auto scene = std::make_shared<Scene>(Colord::black());
    scene->add(std::make_shared<Triangle>(Vector3d(-4.0, -3.0, 0.0), Vector3d(4.0, -3.0, 0.0),
                                          Vector3d(-4.0, 3.0, 0.0)));
    scene->add(std::make_shared<Triangle>(Vector3d(4.0, -3.0, 0.0), Vector3d(4.0, 3.0, 0.0),
                                          Vector3d(-4.0, 3.0, 0.0)));
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithRepeatedQuadrantTriangles(int repeats) {
    auto scene = std::make_shared<Scene>(Colord::black());
    const std::array<std::pair<Vector3d, Vector3d>, 4> quadrants{
      std::make_pair(Vector3d(-4.0, -3.0, 0.0), Vector3d(0.0, 0.0, 0.0)),
      std::make_pair(Vector3d(0.0, -3.0, 0.0), Vector3d(4.0, 0.0, 0.0)),
      std::make_pair(Vector3d(-4.0, 0.0, 0.0), Vector3d(0.0, 3.0, 0.0)),
      std::make_pair(Vector3d(0.0, 0.0, 0.0), Vector3d(4.0, 3.0, 0.0))};
    for (int i = 0; i != repeats; ++i) {
      for (const auto& quadrant : quadrants) {
        const Vector3d& min = quadrant.first;
        const Vector3d& max = quadrant.second;
        scene->add(std::make_shared<Triangle>(min, Vector3d(max.x(), min.y(), 0.0),
                                              Vector3d(min.x(), max.y(), 0.0)));
        scene->add(std::make_shared<Triangle>(Vector3d(max.x(), min.y(), 0.0), max,
                                              Vector3d(min.x(), max.y(), 0.0)));
      }
    }
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithBackFacingTriangle() {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0), Vector3d(0, 1, 0)));
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithFrontFacingTriangle() {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(0, 1, 0), Vector3d(1, -1, 0)));
    return scene;
  }

  static std::shared_ptr<Scene>
  sceneWithMaterialFrontFacingTriangle(std::shared_ptr<render::Material> material) {
    auto scene = std::make_shared<Scene>(Colord::black());
    auto triangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(0, 1, 0), Vector3d(1, -1, 0));
    triangle->setMaterial(std::move(material));
    scene->add(triangle);
    return scene;
  }

  static std::shared_ptr<Scene>
  sceneWithMaterialBackFacingTriangle(std::shared_ptr<render::Material> material) {
    auto scene = std::make_shared<Scene>(Colord::black());
    auto triangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0), Vector3d(0, 1, 0));
    triangle->setMaterial(std::move(material));
    scene->add(triangle);
    return scene;
  }

  static std::shared_ptr<Scene>
  sceneWithMeshBackFacingTriangle(std::shared_ptr<render::Material> material,
                                  Mesh::FaceMetadata::WindingReliability windingReliability) {
    Mesh mesh;
    mesh.addVertex(Vector3d(-1, -1, 0), Vector3d(0, 0, -1));
    mesh.addVertex(Vector3d(1, -1, 0), Vector3d(0, 0, -1));
    mesh.addVertex(Vector3d(0, 1, 0), Vector3d(0, 0, -1));
    Mesh::FaceMetadata metadata;
    metadata.windingReliability = windingReliability;
    mesh.addFace({0, 1, 2}, metadata);

    auto scene = std::make_shared<Scene>(Colord::black());
    auto primitive =
      std::make_shared<MeshPrimitive>(std::move(mesh), MeshPrimitive::NormalMode::Flat);
    primitive->setMaterial(std::move(material));
    scene->add(primitive);
    return scene;
  }

  static TrackedTriangleScene sceneWithTrackedFrontFacingTriangle() {
    TrackedTriangleScene result;
    result.scene = std::make_shared<Scene>(Colord::white());
    result.triangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(0, 1, 0), Vector3d(1, -1, 0));
    result.material = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    result.triangle->setMaterial(result.material);
    result.scene->add(result.triangle);
    return result;
  }

  static std::shared_ptr<Scene>
  sceneWithTexturedFrontFacingTriangle(std::shared_ptr<Texturec> texture) {
    auto scene = std::make_shared<Scene>(Colord::white());
    auto triangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(0, 1, 0), Vector3d(1, -1, 0));
    triangle->setMaterial(std::make_shared<MatteMaterial>(std::move(texture)));
    scene->add(triangle);
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithTexturedFrontFacingTriangle() {
    return sceneWithTexturedFrontFacingTriangle(std::make_shared<render::UVColorTexture>());
  }

  static std::shared_ptr<Scene> sceneWithSlopedTriangle() {
    auto scene = std::make_shared<Scene>(Colord::black());
    scene->add(
      std::make_shared<Triangle>(Vector3d(-2, -2, 0), Vector3d(2, -2, 2), Vector3d(0, 2, 4)));
    return scene;
  }

  static void expectCenterLooksLikeTriangleUV(const Colord& color) {
    EXPECT_GT(color.r(), 0.35);
    EXPECT_LT(color.r(), 0.65);
    EXPECT_GT(color.g(), 0.15);
    EXPECT_LT(color.g(), 0.35);
    EXPECT_DOUBLE_EQ(0.0, color.b());
  }

  static void configureScreenSpaceEdgeTriangle(Rasterizer& engine) {
    engine.setVertexShader([](const Rasterizer::VertexInput& vertex) {
      Vector3d screen(32, 16, 1);
      if (vertex.worldPosition.x() < -0.5) {
        screen = Vector3d(16, 16, 1);
      } else if (vertex.worldPosition.x() > 0.5) {
        screen = Vector3d(16, 32, 1);
      }
      return Rasterizer::VertexOutput{vertex.worldPosition, vertex.normal, vertex.uv,
                                      vertex.clipPosition, screen};
    });
    engine.setFragmentShader([](const Rasterizer::FragmentInput&) { return Colord::white(); });
  }

  static void configureScreenSpaceQuad(Rasterizer& engine) {
    engine.setVertexShader([](const Rasterizer::VertexInput& vertex) {
      const double x = vertex.worldPosition.x() < 0.0 ? 8.0 : 24.0;
      const double y = vertex.worldPosition.y() < 0.0 ? 8.0 : 24.0;
      return Rasterizer::VertexOutput{vertex.worldPosition, vertex.normal, vertex.uv,
                                      vertex.clipPosition, Vector3d(x, y, 1.0)};
    });
  }

  static void configureScreenSpaceSubpixelTriangle(Rasterizer& engine, double rightX) {
    engine.setVertexShader([=](const Rasterizer::VertexInput& vertex) {
      Vector3d screen(8.0, 24.0, 1.0);
      if (vertex.worldPosition.x() < -0.5) {
        screen = Vector3d(8.0, 8.0, 1.0);
      } else if (vertex.worldPosition.x() > 0.5) {
        screen = Vector3d(rightX, 8.0, 1.0);
      }
      return Rasterizer::VertexOutput{vertex.worldPosition, vertex.normal, vertex.uv,
                                      vertex.clipPosition, screen};
    });
    engine.setFragmentShader([](const Rasterizer::FragmentInput&) { return Colord::white(); });
  }

  static void configureScreenSpaceMSAASubpixelTriangle(Rasterizer& engine) {
    engine.setVertexShader([](const Rasterizer::VertexInput& vertex) {
      Vector3d screen(16.0, 32.0, 1.0);
      if (vertex.worldPosition.x() < -0.5) {
        screen = Vector3d(16.0, 0.0, 1.0);
      } else if (vertex.worldPosition.x() > 0.5) {
        screen = Vector3d(32.0, 0.0, 1.0);
      }
      return Rasterizer::VertexOutput{vertex.worldPosition, vertex.normal, vertex.uv,
                                      vertex.clipPosition, screen};
    });
    engine.setFragmentShader([](const Rasterizer::FragmentInput&) { return Colord::white(); });
  }

  static void configureOversizedScreenTriangle(Rasterizer& engine) {
    engine.setVertexShader([](const Rasterizer::VertexInput& vertex) {
      Vector3d screen(0.0, 64.0, 1.0);
      if (vertex.worldPosition.y() > 0.5) {
        screen = Vector3d(0.0, 0.0, 1.0);
      } else if (vertex.worldPosition.x() > 0.5) {
        screen = Vector3d(64.0, 0.0, 1.0);
      }
      return Rasterizer::VertexOutput{vertex.worldPosition, vertex.normal, vertex.uv,
                                      vertex.clipPosition, screen};
    });
    engine.setFragmentShader([](const Rasterizer::FragmentInput&) { return Colord::white(); });
  }

  static std::shared_ptr<Scene> sceneWithOverlappingTriangles() {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(0, 1, 0), Vector3d(1, -1, 0)));
    scene->add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 1), Vector3d(0, 1, 1), Vector3d(1, -1, 1)));
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithDuplicateTriangles() {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(0, 1, 0), Vector3d(1, -1, 0)));
    scene->add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(0, 1, 0), Vector3d(1, -1, 0)));
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithAdjacentQuadTriangles() {
    auto scene = std::make_shared<Scene>(Colord::white());
    const Vector3d p00(-1.0, -1.0, 0.0);
    const Vector3d p10(1.0, -1.0, 0.0);
    const Vector3d p01(-1.0, 1.0, 0.0);
    const Vector3d p11(1.0, 1.0, 0.0);
    scene->add(std::make_shared<Triangle>(p00, p10, p01));
    scene->add(std::make_shared<Triangle>(p10, p11, p01));
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithOversizedScreenTriangle() {
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(0, 1, 0), Vector3d(1, -1, 0)));
    return scene;
  }

  static engine::raster::detail::RasterTriangle screenTriangle(double x0, double y0, double x1,
                                                               double y1, double x2, double y2) {
    engine::raster::detail::RasterTriangle triangle{};
    triangle.vertices[0].x = x0;
    triangle.vertices[0].y = y0;
    triangle.vertices[1].x = x1;
    triangle.vertices[1].y = y1;
    triangle.vertices[2].x = x2;
    triangle.vertices[2].y = y2;
    return triangle;
  }

  static std::shared_ptr<Scene> sceneWithFrontOccluderAndDenseBackLayer(int backRectangleCount) {
    auto scene = std::make_shared<Scene>(Colord::black());
    auto addRectangle = [&](double z) {
      scene->add(std::make_shared<Triangle>(Vector3d(-100.0, -100.0, z), Vector3d(100.0, -100.0, z),
                                            Vector3d(-100.0, 100.0, z)));
      scene->add(std::make_shared<Triangle>(Vector3d(100.0, -100.0, z), Vector3d(100.0, 100.0, z),
                                            Vector3d(-100.0, 100.0, z)));
    };

    addRectangle(0.0);
    for (int i = 0; i != backRectangleCount; ++i) {
      addRectangle(1.0);
    }
    return scene;
  }

  static std::shared_ptr<Scene> sceneWithDirectionalShadowCaster() {
    auto scene = std::make_shared<Scene>(Colord(0.1, 0.1, 0.1));

    auto wall = std::make_shared<Rectangle>(Vector3d(-2.0, -2.0, 1.0), Vector3d(0.0, 4.0, 0.0),
                                            Vector3d(4.0, 0.0, 0.0));
    wall->setMaterial(matte(Colord::white()));
    scene->add(wall);

    auto caster = std::make_shared<Box>(Vector3d(0.0, 0.0, 0.0), Vector3d(0.35, 0.35, 0.35));
    caster->setMaterial(matte(Colord::white()));
    scene->add(caster);

    scene->addLight(std::make_shared<DirectionalLight>(Vector3d(-0.5, 0.2, -1.0), Colord::white()));
    return scene;
  }

  static Colord colorAtWorldPoint(const Buffer<Colord>& buffer,
                                  const std::shared_ptr<PinholeCamera>& cam,
                                  const Vector3d& point) {
    const Vector2d screen = cam->projectPoint(point);
    EXPECT_TRUE(screen.isDefined());
    if (screen.isUndefined())
      return Colord::black();
    const int x = static_cast<int>(std::lround(screen.x()));
    const int y = static_cast<int>(std::lround(screen.y()));
    EXPECT_GE(x, 0);
    EXPECT_GE(y, 0);
    EXPECT_LT(x, buffer.width());
    EXPECT_LT(y, buffer.height());
    if (x < 0 || y < 0 || x >= buffer.width() || y >= buffer.height())
      return Colord::black();
    return buffer[y][x];
  }

  static engine::raster::detail::DirectionalShadowMap
  syntheticShadowMap(double constantBias, double slopeBias, int filterRadius = 0,
                     Rasterizer::ShadowFilterMode filterMode = Rasterizer::ShadowFilterMode::PCF) {
    const Vector3d lightDirection(0.0, 0.0, -1.0);
    auto shadowCamera = std::make_shared<engine::raster::detail::DirectionalShadowCamera>(
      Vector3d::null, lightDirection, 1.0);
    shadowCamera->setViewPlane(std::make_shared<ViewPlane>());
    shadowCamera->viewPlane()->setup(Matrix4d(), Recti(4, 4));

    auto depthBuffer = std::make_unique<Buffer<double>>(4, 4);
    depthBuffer->clear(std::numeric_limits<double>::infinity());
    (*depthBuffer)[2][2] = 1.95;

    std::vector<engine::raster::detail::DirectionalShadowCascade> cascades;
    cascades.push_back({std::move(shadowCamera), std::move(depthBuffer), 0.0, 1.0});
    return engine::raster::detail::DirectionalShadowMap(
      nullptr, nullptr, std::move(cascades), constantBias, slopeBias, filterRadius, filterMode);
  }

  TEST(Rasterizer, EmptySceneRendersBackgroundOnly) {
    auto scene = std::make_shared<Scene>(Colord::white());
    Rasterizer engine(angledCamera(), scene);
    engine.setBackgroundColor(Colord(0.1, 0.2, 0.3));
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(64 * 64, countPixels(buffer, Colord(0.1, 0.2, 0.3)));
  }

  TEST(Rasterizer, SceneWithBoxFillsSomePixels) {
    Rasterizer engine(angledCamera(), sceneWithBox());
    Buffer<Colord> buffer(128, 128);
    engine.render(buffer);

    const int filled = countPixelsNotEqualTo(buffer, Colord::black());
    EXPECT_GT(filled, 0);
    // Filled region should be substantially larger than the
    // wireframe-engine output for the same scene — a filled box
    // covers a hexagonal area, not just edges.
    EXPECT_GT(filled, 100);
  }

  TEST(Rasterizer, SceneWithSphereFillsManyMorePixelsThanWireframeOutline) {
    // The sphere's tessellated mesh has ~256 triangles at lod=0;
    // a filled rasterizer covers the silhouette disk while a
    // wireframe draws only edges. We don't compare against
    // Wireframe directly here (that would couple the test to two
    // engines) — just assert the filled count is "large", indicating
    // the interior is being filled.
    Rasterizer engine(angledCamera(), sceneWithSphere());
    Buffer<Colord> buffer(128, 128);
    engine.render(buffer);

    const int filled = countPixelsNotEqualTo(buffer, Colord::black());
    // 128×128 framebuffer with a unit sphere viewed from (2,2,-5)
    // produces a small silhouette disk (≈π·8² = 200 pixels). The
    // claim is "the interior is filled" — much larger than what an
    // outline-only renderer would produce for the same projection.
    EXPECT_GT(filled, 150);
  }

  TEST(Rasterizer, SceneWithCurveTubeFillsSomePixels) {
    Rasterizer engine(standardCamera(), sceneWithCurveTube());
    Buffer<Colord> buffer(128, 128);

    engine.render(buffer);

    EXPECT_GT(countPixelsNotEqualTo(buffer, Colord::black()), 0);
  }

  TEST(Rasterizer, ThinLensCameraFallsBackToPinholeProjection) {
    auto cam = std::make_shared<ThinLensCamera>(Vector3d(2, 2, -5), Vector3d::null);
    cam->setApertureRadius(0.8);
    cam->setFocalDistance(2.0);
    Rasterizer engine(cam, sceneWithSphere());
    Buffer<Colord> buffer(128, 128);
    engine.render(buffer);

    const int filled = countPixelsNotEqualTo(buffer, Colord::black());
    EXPECT_GT(filled, 150);
  }

  TEST(Rasterizer, BackgroundColorIsConfigurable) {
    auto scene = std::make_shared<Scene>(Colord::white());
    Rasterizer engine(angledCamera(), scene);
    engine.setBackgroundColor(Colord(0.5, 0.0, 0.5));
    Buffer<Colord> buffer(32, 32);

    engine.render(buffer);

    EXPECT_EQ(32 * 32, countPixels(buffer, Colord(0.5, 0.0, 0.5)));
  }

  TEST(Rasterizer, HigherLodProducesMoreOrEqualFilledPixels) {
    // For a Sphere primitive, higher LOD means denser triangulation
    // — but the silhouette area is bounded by the sphere's actual
    // projected size, so the filled region grows toward but doesn't
    // exceed that bound. Looser invariant: high-LOD render fills at
    // least as many pixels as low-LOD for this centered sphere; not a
    // strict mathematical claim for arbitrary scenes.
    auto scene = sceneWithSphere();

    Rasterizer engineLow(angledCamera(), scene);
    engineLow.setLod(0);
    Buffer<Colord> bufferLow(256, 256);
    engineLow.render(bufferLow);
    const int filledLow = countPixelsNotEqualTo(bufferLow, Colord::black());

    Rasterizer engineHigh(angledCamera(), scene);
    engineHigh.setLod(2);
    Buffer<Colord> bufferHigh(256, 256);
    engineHigh.render(bufferHigh);
    const int filledHigh = countPixelsNotEqualTo(bufferHigh, Colord::black());

    EXPECT_GE(filledHigh, filledLow);
  }

  TEST(Rasterizer, FrustumBoundsOutsideViewSkipTessellation) {
    int tessellateCalls = 0;
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(std::make_shared<CountingPrimitive>(
      BoundingBoxd(Vector3d(1000.0, -1.0, 0.0), Vector3d(1001.0, 1.0, 1.0)), &tessellateCalls));
    Rasterizer engine(standardCamera(), scene);
    Buffer<Colord> buffer(32, 32);

    engine.render(buffer);

    EXPECT_EQ(0, tessellateCalls);
  }

  TEST(Rasterizer, FrustumBoundsKeepVisiblePrimitiveForTessellation) {
    int tessellateCalls = 0;
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(std::make_shared<CountingPrimitive>(
      BoundingBoxd(Vector3d(-1.0, -1.0, 0.0), Vector3d(1.0, 1.0, 1.0)), &tessellateCalls));
    Rasterizer engine(standardCamera(), scene);
    Buffer<Colord> buffer(32, 32);

    engine.render(buffer);

    EXPECT_EQ(1, tessellateCalls);
  }

  TEST(Rasterizer, FrustumBoundsOutsideGroupSkipFlatteningAndTessellation) {
    int flattenCalls = 0;
    int tessellateCalls = 0;
    auto scene = std::make_shared<Scene>(Colord::white());
    auto group = std::make_shared<CountingComposite>(&flattenCalls);
    group->add(std::make_shared<CountingPrimitive>(
      BoundingBoxd(Vector3d(1000.0, -1.0, 0.0), Vector3d(1001.0, 1.0, 1.0)), &tessellateCalls));
    scene->add(group);
    Rasterizer engine(standardCamera(), scene);
    Buffer<Colord> buffer(32, 32);

    engine.render(buffer);

    EXPECT_EQ(0, flattenCalls);
    EXPECT_EQ(0, tessellateCalls);
  }

  TEST(Rasterizer, FrustumBoundsFallbackTraversalStillSkipsTessellation) {
    int flattenCalls = 0;
    int tessellateCalls = 0;
    auto scene = std::make_shared<NonSpatialScene>();
    auto group = std::make_shared<CountingComposite>(&flattenCalls);
    group->add(std::make_shared<CountingPrimitive>(
      BoundingBoxd(Vector3d(1000.0, -1.0, 0.0), Vector3d(1001.0, 1.0, 1.0)), &tessellateCalls));
    scene->add(group);
    Rasterizer engine(standardCamera(), scene);
    Buffer<Colord> buffer(32, 32);

    engine.render(buffer);

    EXPECT_EQ(1, flattenCalls);
    EXPECT_EQ(0, tessellateCalls);
  }

  TEST(Rasterizer, FrustumBoundsCullingKeepsVertexShaderPathConservative) {
    int tessellateCalls = 0;
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(std::make_shared<CountingPrimitive>(
      BoundingBoxd(Vector3d(1000.0, -1.0, 0.0), Vector3d(1001.0, 1.0, 1.0)), &tessellateCalls));
    Rasterizer engine(standardCamera(), scene);
    engine.setVertexShader([](const Rasterizer::VertexInput& vertex) {
      return Rasterizer::VertexOutput{vertex.worldPosition, vertex.normal, vertex.uv,
                                      vertex.clipPosition, vertex.screenPosition};
    });
    Buffer<Colord> buffer(32, 32);

    engine.render(buffer);

    EXPECT_EQ(1, tessellateCalls);
  }

  TEST(Rasterizer, VisibilitySetSkipsRejectedLeafBeforeTessellation) {
    int visibleTessellateCalls = 0;
    int rejectedTessellateCalls = 0;
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(std::make_shared<CountingPrimitive>(
      BoundingBoxd(Vector3d(-1.0, -1.0, 0.0), Vector3d(1.0, 1.0, 1.0)), &visibleTessellateCalls));
    scene->add(std::make_shared<CountingPrimitive>(
      BoundingBoxd(Vector3d(-1.0, -1.0, 0.0), Vector3d(1.0, 1.0, 1.0)), &rejectedTessellateCalls));

    auto visibilitySet = std::make_shared<RasterVisibilitySet>();
    visibilitySet->addVisibleLeaf(0);
    visibilitySet->addRejectedLeaf(RasterVisibilitySet::RejectionReason::Frustum, 0);

    Rasterizer engine(standardCamera(), scene);
    engine.setVisibilitySet(visibilitySet);
    engine.setVertexShader([](const Rasterizer::VertexInput& vertex) {
      return Rasterizer::VertexOutput{vertex.worldPosition, vertex.normal, vertex.uv,
                                      vertex.clipPosition, vertex.screenPosition};
    });
    Buffer<Colord> buffer(32, 32);

    engine.render(buffer);

    EXPECT_EQ(1, visibleTessellateCalls);
    EXPECT_EQ(0, rejectedTessellateCalls);
  }

  TEST(Rasterizer, VisibilitySetOrdersVisibleLeavesBeforeTessellation) {
    std::vector<int> tessellationOrder;
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(std::make_shared<RecordingPrimitive>(0, &tessellationOrder));
    scene->add(std::make_shared<RecordingPrimitive>(1, &tessellationOrder));
    scene->add(std::make_shared<RecordingPrimitive>(2, &tessellationOrder));

    auto visibilitySet = std::make_shared<RasterVisibilitySet>();
    visibilitySet->addVisibleLeaf(0);
    visibilitySet->addVisibleLeaf(0);
    visibilitySet->addVisibleLeaf(0);
    visibilitySet->setVisibleLeafOrder({2, 0});

    Rasterizer engine(standardCamera(), scene);
    engine.setVisibilitySet(visibilitySet);
    Buffer<Colord> buffer(32, 32);

    engine.render(buffer);

    EXPECT_EQ((std::vector<int>{2, 0, 1}), tessellationOrder);
  }

  TEST(Rasterizer, HandlesNullSceneGracefully) {
    Rasterizer engine(angledCamera(), nullptr);
    engine.setBackgroundColor(Colord(0.1, 0.1, 0.1));
    Buffer<Colord> buffer(32, 32);

    engine.render(buffer);

    EXPECT_EQ(32 * 32, countPixels(buffer, Colord(0.1, 0.1, 0.1)));
  }

  TEST(Rasterizer, CancelStopsFurtherDrawing) {
    Rasterizer engine(angledCamera(), sceneWithBox());
    engine.setBackgroundColor(Colord::black());
    Buffer<Colord> buffer(64, 64);

    engine.cancel();
    engine.render(buffer);

    // Background still cleared; no triangles drawn.
    EXPECT_EQ(64 * 64, countPixels(buffer, Colord::black()));
  }

  TEST(Rasterizer, ZBufferCullsOccludedGeometryAddedAfterTheOccluder) {
    // Scene 1: just a near sphere.
    // Scene 2: the same near sphere added first (so its fallback
    //          face colors match scene 1), plus a large box
    //          rendered behind it. The box would overdraw the
    //          sphere's center pixels without depth-testing. With
    //          the Z-buffer, the box's center pixels fail the depth
    //          test and the sphere's color stays.
    //
    // The sphere is added first in BOTH scenes, so its fallback face
    // colors are identical when no material is attached. That makes
    // pixel-equality at the center a stable assertion — without the
    // depth test this expectation would fail.

    auto cam = std::make_shared<PinholeCamera>(Vector3d(0, 0, -8), Vector3d::null);

    auto sceneAlone = std::make_shared<Scene>(Colord::white());
    sceneAlone->add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));

    auto sceneWithBack = std::make_shared<Scene>(Colord::white());
    sceneWithBack->add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0)); // near, idx 0..N-1
    sceneWithBack->add(
      std::make_shared<Box>(Vector3d(0, 0, 10), Vector3d(5, 5, 0.1))); // far back wall

    Rasterizer eAlone(cam, sceneAlone);
    Rasterizer eWithBack(cam, sceneWithBack);

    Buffer<Colord> bAlone(64, 64);
    Buffer<Colord> bWithBack(64, 64);
    eAlone.render(bAlone);
    eWithBack.render(bWithBack);

    // Center pixel: covered by the near sphere in both renders. With
    // the Z-buffer, the back box's pixels at the center fail the
    // depth test; without it, the box would overdraw the sphere
    // there because it's added second in mesh order.
    EXPECT_EQ(bAlone[32][32], bWithBack[32][32])
      << "Center pixel changed when an occluded back wall was added — "
      << "Z-buffer is failing to cull the farther geometry.";
    EXPECT_FALSE(bAlone[32][32] == Colord::black())
      << "Center pixel should be colored by the near sphere.";
  }

  TEST(Rasterizer, DepthStencilAndShaderDefaultsMatchFixedPipeline) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());

    EXPECT_EQ(Rasterizer::DepthFunc::Less, engine.depthFunc());
    EXPECT_TRUE(engine.depthWriteEnabled());
    EXPECT_FALSE(engine.stencilTestEnabled());
    EXPECT_EQ(Rasterizer::StencilFunc::Always, engine.stencilFunc());
    EXPECT_EQ(Rasterizer::StencilOp::Keep, engine.stencilFailOp());
    EXPECT_EQ(Rasterizer::StencilOp::Keep, engine.stencilDepthFailOp());
    EXPECT_EQ(Rasterizer::StencilOp::Keep, engine.stencilPassOp());
    EXPECT_FALSE(engine.alphaTestEnabled());
    EXPECT_EQ(Rasterizer::AlphaFunc::Always, engine.alphaFunc());
    EXPECT_DOUBLE_EQ(0.0, engine.alphaReference());
    EXPECT_EQ(Rasterizer::ColorWriteAll, engine.colorWriteMask());
    EXPECT_FALSE(engine.blendingEnabled());
    EXPECT_EQ(Rasterizer::BlendFactor::One, engine.sourceBlendFactor());
    EXPECT_EQ(Rasterizer::BlendFactor::Zero, engine.destinationBlendFactor());
    EXPECT_EQ(Rasterizer::BlendOp::Add, engine.blendOp());
    EXPECT_EQ(Colord::white(), engine.blendConstantColor());
    EXPECT_DOUBLE_EQ(1.0, engine.blendConstantAlpha());
    EXPECT_FALSE(static_cast<bool>(engine.vertexShader()));
    EXPECT_FALSE(static_cast<bool>(engine.fragmentShader()));
    EXPECT_EQ(1, engine.msaaSamples());
    EXPECT_EQ(Rasterizer::MSAAShadingMode::PerSample, engine.msaaShadingMode());
    EXPECT_DOUBLE_EQ(0.1, engine.nearClipDepth());
    EXPECT_TRUE(std::isinf(engine.farClipDepth()));
    EXPECT_EQ(Rasterizer::PostProcessAA::None, engine.postProcessAA());
    EXPECT_DOUBLE_EQ(0.1, engine.temporalCurrentFrameWeight());
    EXPECT_FALSE(engine.temporalHistoryValid());
    EXPECT_EQ(0, engine.temporalFrameIndex());
    EXPECT_DOUBLE_EQ(0.0, engine.depthBias());
    EXPECT_FALSE(engine.shadowMapsEnabled());
    EXPECT_EQ(256, engine.shadowMapSize());
    EXPECT_EQ(1, engine.shadowCascadeCount());
    EXPECT_DOUBLE_EQ(0.5, engine.shadowCascadeSplitLambda());
    EXPECT_DOUBLE_EQ(1e-3, engine.shadowBias());
    EXPECT_DOUBLE_EQ(0.0, engine.shadowSlopeBias());
    EXPECT_EQ(0, engine.shadowFilterRadius());
    EXPECT_EQ(Rasterizer::ShadowFilterMode::PCF, engine.shadowFilterMode());
    EXPECT_FALSE(engine.viewportEnabled());
    EXPECT_EQ(0, engine.viewportRect().width());
    EXPECT_EQ(0, engine.viewportRect().height());
    EXPECT_FALSE(engine.scissorTestEnabled());
    EXPECT_EQ(0, engine.scissorRect().width());
    EXPECT_EQ(0, engine.scissorRect().height());
    EXPECT_EQ(Rasterizer::AttachmentLoadOp::Clear, engine.colorLoadOp());
    EXPECT_EQ(Rasterizer::AttachmentStoreOp::Store, engine.colorStoreOp());
    EXPECT_EQ(Rasterizer::AttachmentLoadOp::Clear, engine.depthLoadOp());
    EXPECT_EQ(Rasterizer::AttachmentStoreOp::Store, engine.depthStoreOp());
    EXPECT_EQ(Rasterizer::AttachmentLoadOp::Clear, engine.stencilLoadOp());
    EXPECT_EQ(Rasterizer::AttachmentStoreOp::Store, engine.stencilStoreOp());
    EXPECT_EQ(nullptr, engine.diagnosticOutputBuffers().depth);
    EXPECT_EQ(nullptr, engine.diagnosticOutputBuffers().worldPosition);
    EXPECT_EQ(nullptr, engine.diagnosticOutputBuffers().normal);
    EXPECT_EQ(nullptr, engine.diagnosticOutputBuffers().primitive);
    EXPECT_EQ(nullptr, engine.diagnosticOutputBuffers().material);
    EXPECT_EQ(nullptr, engine.diagnosticOutputBuffers().face);
    EXPECT_EQ(nullptr, engine.diagnosticOutputBuffers().stencil);
    EXPECT_EQ(nullptr, engine.attachmentBuffers().depth);
    EXPECT_EQ(nullptr, engine.attachmentBuffers().stencil);
  }

  TEST(Rasterizer, ClonePreservesPostProcessAAAndShadowFilterMode) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setPostProcessAA(Rasterizer::PostProcessAA::SMAA);
    engine.setMSAAShadingMode(Rasterizer::MSAAShadingMode::PerFragment);
    engine.setPostProcessAA(Rasterizer::PostProcessAA::TAA);
    engine.setTemporalCurrentFrameWeight(0.35);
    engine.setNearClipDepth(0.5);
    engine.setFarClipDepth(25.0);
    engine.setShadowCascadeCount(3);
    engine.setShadowCascadeSplitLambda(0.75);
    engine.setShadowSlopeBias(0.02);
    engine.setShadowFilterMode(Rasterizer::ShadowFilterMode::PCSS);
    engine.setColorWriteMask(Rasterizer::ColorWriteGreen);
    engine.setBlendingEnabled(true);
    engine.setBlendFactors(Rasterizer::BlendFactor::ConstantAlpha,
                           Rasterizer::BlendFactor::OneMinusConstantAlpha);
    engine.setBlendOp(Rasterizer::BlendOp::ReverseSubtract);
    engine.setBlendConstant(Colord(0.25, 0.5, 0.75), 0.35);
    engine.setViewportRect(8, 10, 40, 42);
    engine.setScissorRect(12, 14, 20, 22);
    engine.setColorLoadOp(Rasterizer::AttachmentLoadOp::Load);
    engine.setColorStoreOp(Rasterizer::AttachmentStoreOp::Discard);
    engine.setDepthBias(-0.125);
    engine.setDepthLoadOp(Rasterizer::AttachmentLoadOp::Load);
    engine.setDepthStoreOp(Rasterizer::AttachmentStoreOp::Discard);
    engine.setStencilLoadOp(Rasterizer::AttachmentLoadOp::Load);
    engine.setStencilStoreOp(Rasterizer::AttachmentStoreOp::Discard);
    engine.setAlphaTestEnabled(true);
    engine.setAlphaFunc(Rasterizer::AlphaFunc::Greater, 0.25);
    Buffer<double> depth(1, 1);
    Buffer<std::uint8_t> stencil(1, 1);
    Rasterizer::DiagnosticOutputBuffers outputs;
    outputs.depth = &depth;
    engine.setDiagnosticOutputBuffers(outputs);
    Rasterizer::AttachmentBuffers attachments;
    attachments.depth = &depth;
    attachments.stencil = &stencil;
    engine.setAttachmentBuffers(attachments);

    auto clone = std::dynamic_pointer_cast<Rasterizer>(engine.cloneForRender());

    ASSERT_NE(nullptr, clone);
    EXPECT_EQ(Rasterizer::MSAAShadingMode::PerFragment, clone->msaaShadingMode());
    EXPECT_EQ(Rasterizer::PostProcessAA::TAA, clone->postProcessAA());
    EXPECT_DOUBLE_EQ(0.35, clone->temporalCurrentFrameWeight());
    EXPECT_DOUBLE_EQ(0.5, clone->nearClipDepth());
    EXPECT_DOUBLE_EQ(25.0, clone->farClipDepth());
    EXPECT_EQ(3, clone->shadowCascadeCount());
    EXPECT_DOUBLE_EQ(0.75, clone->shadowCascadeSplitLambda());
    EXPECT_DOUBLE_EQ(0.02, clone->shadowSlopeBias());
    EXPECT_EQ(Rasterizer::ShadowFilterMode::PCSS, clone->shadowFilterMode());
    EXPECT_EQ(Rasterizer::ColorWriteGreen, clone->colorWriteMask());
    EXPECT_TRUE(clone->blendingEnabled());
    EXPECT_EQ(Rasterizer::BlendFactor::ConstantAlpha, clone->sourceBlendFactor());
    EXPECT_EQ(Rasterizer::BlendFactor::OneMinusConstantAlpha, clone->destinationBlendFactor());
    EXPECT_EQ(Rasterizer::BlendOp::ReverseSubtract, clone->blendOp());
    EXPECT_EQ(Colord(0.25, 0.5, 0.75), clone->blendConstantColor());
    EXPECT_DOUBLE_EQ(0.35, clone->blendConstantAlpha());
    EXPECT_TRUE(clone->viewportEnabled());
    EXPECT_EQ(8, clone->viewportRect().left());
    EXPECT_EQ(10, clone->viewportRect().top());
    EXPECT_EQ(40, clone->viewportRect().width());
    EXPECT_EQ(42, clone->viewportRect().height());
    EXPECT_TRUE(clone->scissorTestEnabled());
    EXPECT_EQ(12, clone->scissorRect().left());
    EXPECT_EQ(14, clone->scissorRect().top());
    EXPECT_EQ(20, clone->scissorRect().width());
    EXPECT_EQ(22, clone->scissorRect().height());
    EXPECT_EQ(Rasterizer::AttachmentLoadOp::Load, clone->colorLoadOp());
    EXPECT_EQ(Rasterizer::AttachmentStoreOp::Discard, clone->colorStoreOp());
    EXPECT_DOUBLE_EQ(-0.125, clone->depthBias());
    EXPECT_EQ(Rasterizer::AttachmentLoadOp::Load, clone->depthLoadOp());
    EXPECT_EQ(Rasterizer::AttachmentStoreOp::Discard, clone->depthStoreOp());
    EXPECT_EQ(Rasterizer::AttachmentLoadOp::Load, clone->stencilLoadOp());
    EXPECT_EQ(Rasterizer::AttachmentStoreOp::Discard, clone->stencilStoreOp());
    EXPECT_TRUE(clone->alphaTestEnabled());
    EXPECT_EQ(Rasterizer::AlphaFunc::Greater, clone->alphaFunc());
    EXPECT_DOUBLE_EQ(0.25, clone->alphaReference());
    EXPECT_EQ(nullptr, clone->diagnosticOutputBuffers().depth);
    EXPECT_EQ(nullptr, clone->diagnosticOutputBuffers().worldPosition);
    EXPECT_EQ(nullptr, clone->attachmentBuffers().depth);
    EXPECT_EQ(nullptr, clone->attachmentBuffers().stencil);
  }

  TEST(Rasterizer, TemporalAATracksHistoryLifecycle) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setPostProcessAA(Rasterizer::PostProcessAA::TAA);
    Buffer<Colord> color(64, 64);

    EXPECT_FALSE(engine.temporalHistoryValid());
    EXPECT_EQ(0, engine.temporalFrameIndex());

    engine.render(color);
    EXPECT_TRUE(engine.temporalHistoryValid());
    EXPECT_EQ(1, engine.temporalFrameIndex());

    engine.render(color);
    EXPECT_TRUE(engine.temporalHistoryValid());
    EXPECT_EQ(2, engine.temporalFrameIndex());

    engine.invalidateTemporalHistory();
    engine.render(color);
    EXPECT_TRUE(engine.temporalHistoryValid());
    EXPECT_EQ(1, engine.temporalFrameIndex());
  }

  TEST(Rasterizer, TemporalAAResetsHistoryWhenRenderTargetResizes) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setPostProcessAA(Rasterizer::PostProcessAA::TAA);
    Buffer<Colord> color64(64, 64);
    Buffer<Colord> color32(32, 32);

    engine.render(color64);
    engine.render(color64);
    EXPECT_EQ(2, engine.temporalFrameIndex());

    engine.render(color32);
    EXPECT_TRUE(engine.temporalHistoryValid());
    EXPECT_EQ(1, engine.temporalFrameIndex());
  }

  TEST(Rasterizer, TemporalAAResetsHistoryWhenCameraChanges) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setPostProcessAA(Rasterizer::PostProcessAA::TAA);
    Buffer<Colord> color(64, 64);

    engine.render(color);
    engine.render(color);
    EXPECT_EQ(2, engine.temporalFrameIndex());

    engine.setCamera(angledCamera());
    engine.render(color);
    EXPECT_TRUE(engine.temporalHistoryValid());
    EXPECT_EQ(1, engine.temporalFrameIndex());
  }

  TEST(Rasterizer, ClonePreservesDisabledScissorRectangle) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setScissorRect(12, 14, 20, 22);
    engine.setScissorTestEnabled(false);

    auto clone = std::dynamic_pointer_cast<Rasterizer>(engine.cloneForRender());

    ASSERT_NE(nullptr, clone);
    EXPECT_FALSE(clone->scissorTestEnabled());
    EXPECT_EQ(12, clone->scissorRect().left());
    EXPECT_EQ(14, clone->scissorRect().top());
    EXPECT_EQ(20, clone->scissorRect().width());
    EXPECT_EQ(22, clone->scissorRect().height());
  }

  TEST(Rasterizer, DiagnosticOutputBuffersCapturePassingFragmentData) {
    auto tracked = sceneWithTrackedFrontFacingTriangle();
    Rasterizer engine(standardCamera(), tracked.scene);
    engine.setStencilTestEnabled(true);
    engine.setStencilFunc(Rasterizer::StencilFunc::Always, 7);
    engine.setStencilOps(Rasterizer::StencilOp::Keep, Rasterizer::StencilOp::Keep,
                         Rasterizer::StencilOp::Replace);

    Buffer<Colord> color(64, 64);
    Buffer<double> depth(64, 64);
    Buffer<Vector3d> worldPosition(64, 64);
    Buffer<Vector3d> normal(64, 64);
    Buffer<const Primitive*> primitive(64, 64);
    Buffer<const Material*> material(64, 64);
    Buffer<std::uint64_t> face(64, 64);
    Buffer<std::uint8_t> stencil(64, 64);
    Buffer<std::uint32_t> coverageCount(64, 64);
    Buffer<std::uint32_t> depthTestCount(64, 64);
    Buffer<std::uint32_t> depthPassCount(64, 64);
    Buffer<std::uint32_t> shadeCount(64, 64);
    Buffer<std::uint32_t> colorWriteCount(64, 64);

    Rasterizer::DiagnosticOutputBuffers outputs;
    outputs.depth = &depth;
    outputs.worldPosition = &worldPosition;
    outputs.normal = &normal;
    outputs.primitive = &primitive;
    outputs.material = &material;
    outputs.face = &face;
    outputs.stencil = &stencil;
    outputs.coverageCount = &coverageCount;
    outputs.depthTestCount = &depthTestCount;
    outputs.depthPassCount = &depthPassCount;
    outputs.shadeCount = &shadeCount;
    outputs.colorWriteCount = &colorWriteCount;
    engine.setDiagnosticOutputBuffers(outputs);

    engine.render(color);

    EXPECT_EQ(engine.depthClearValue(), depth[0][0]);
    EXPECT_TRUE(worldPosition[0][0].isUndefined());
    EXPECT_TRUE(normal[0][0].isUndefined());
    EXPECT_EQ(nullptr, primitive[0][0]);
    EXPECT_EQ(nullptr, material[0][0]);
    EXPECT_EQ(std::numeric_limits<std::uint64_t>::max(), face[0][0]);
    EXPECT_EQ(0, stencil[0][0]);
    EXPECT_EQ(0u, coverageCount[0][0]);
    EXPECT_EQ(0u, depthTestCount[0][0]);
    EXPECT_EQ(0u, depthPassCount[0][0]);
    EXPECT_EQ(0u, shadeCount[0][0]);
    EXPECT_EQ(0u, colorWriteCount[0][0]);

    EXPECT_TRUE(std::isfinite(depth[32][32]));
    EXPECT_GT(depth[32][32], 0.0);
    EXPECT_TRUE(worldPosition[32][32].isDefined());
    EXPECT_NEAR(0.0, worldPosition[32][32].z(), 1e-9);
    EXPECT_NEAR(1.0, normal[32][32].length(), 1e-9);
    EXPECT_EQ(tracked.triangle.get(), primitive[32][32]);
    EXPECT_EQ(tracked.material.get(), material[32][32]);
    EXPECT_EQ(0u, face[32][32]);
    EXPECT_EQ(7, stencil[32][32]);
    EXPECT_EQ(1u, coverageCount[32][32]);
    EXPECT_EQ(1u, depthTestCount[32][32]);
    EXPECT_EQ(1u, depthPassCount[32][32]);
    EXPECT_EQ(1u, shadeCount[32][32]);
    EXPECT_EQ(1u, colorWriteCount[32][32]);
  }

  TEST(Rasterizer, DiagnosticCounterBuffersCaptureRasterWorkBeforeVisibility) {
    Rasterizer engine(standardCamera(), sceneWithDuplicateTriangles());
    engine.setQueueSize(1);

    Buffer<Colord> color(64, 64);
    Buffer<std::uint32_t> coverageCount(64, 64);
    Buffer<std::uint32_t> depthTestCount(64, 64);
    Buffer<std::uint32_t> depthPassCount(64, 64);
    Buffer<std::uint32_t> shadeCount(64, 64);
    Buffer<std::uint32_t> colorWriteCount(64, 64);

    Rasterizer::DiagnosticOutputBuffers outputs;
    outputs.coverageCount = &coverageCount;
    outputs.depthTestCount = &depthTestCount;
    outputs.depthPassCount = &depthPassCount;
    outputs.shadeCount = &shadeCount;
    outputs.colorWriteCount = &colorWriteCount;
    engine.setDiagnosticOutputBuffers(outputs);

    engine.render(color);

    EXPECT_EQ(0u, coverageCount[0][0]);
    EXPECT_EQ(0u, depthTestCount[0][0]);
    EXPECT_EQ(0u, depthPassCount[0][0]);
    EXPECT_EQ(0u, shadeCount[0][0]);
    EXPECT_EQ(0u, colorWriteCount[0][0]);

    EXPECT_EQ(2u, coverageCount[32][32]);
    EXPECT_EQ(2u, depthTestCount[32][32]);
    EXPECT_EQ(1u, depthPassCount[32][32]);
    EXPECT_EQ(1u, shadeCount[32][32]);
    EXPECT_EQ(1u, colorWriteCount[32][32]);
  }

  TEST(Rasterizer, VisibilitySetReducesDiagnosticCounterWork) {
    Rasterizer engine(standardCamera(), sceneWithDuplicateTriangles());
    engine.setQueueSize(1);
    auto visibilitySet = std::make_shared<RasterVisibilitySet>();
    visibilitySet->addVisibleLeaf(1, 1);
    visibilitySet->addRejectedLeaf(RasterVisibilitySet::RejectionReason::Frustum, 1, 1);
    engine.setVisibilitySet(visibilitySet);

    Buffer<Colord> color(64, 64);
    Buffer<std::uint32_t> coverageCount(64, 64);
    Buffer<std::uint32_t> depthTestCount(64, 64);
    Buffer<std::uint32_t> depthPassCount(64, 64);
    Buffer<std::uint32_t> shadeCount(64, 64);
    Buffer<std::uint32_t> colorWriteCount(64, 64);

    Rasterizer::DiagnosticOutputBuffers outputs;
    outputs.coverageCount = &coverageCount;
    outputs.depthTestCount = &depthTestCount;
    outputs.depthPassCount = &depthPassCount;
    outputs.shadeCount = &shadeCount;
    outputs.colorWriteCount = &colorWriteCount;
    engine.setDiagnosticOutputBuffers(outputs);

    engine.render(color);

    EXPECT_EQ(1u, coverageCount[32][32]);
    EXPECT_EQ(1u, depthTestCount[32][32]);
    EXPECT_EQ(1u, depthPassCount[32][32]);
    EXPECT_EQ(1u, shadeCount[32][32]);
    EXPECT_EQ(1u, colorWriteCount[32][32]);
  }

  TEST(Rasterizer, MetricsCaptureRasterWorkWithoutDiagnosticOutputBuffers) {
    Rasterizer engine(standardCamera(), sceneWithDuplicateTriangles());
    engine.setQueueSize(4);

    Buffer<Colord> color(64, 64);
    engine.render(color);

    const auto& metrics = engine.lastMetrics();
    EXPECT_EQ(2u, metrics.input.leafPrimitiveCount);
    EXPECT_EQ(2u, metrics.input.meshCount);
    EXPECT_EQ(0u, metrics.input.materialCount);
    EXPECT_EQ(0u, metrics.input.lightCount);
    EXPECT_EQ(6u, metrics.tessellation.generatedMeshVertices);
    EXPECT_EQ(2u, metrics.tessellation.generatedMeshFaces);
    EXPECT_EQ(2u, metrics.tessellation.preparedTrianglesBeforeCulling);
    EXPECT_EQ(0u, metrics.tessellation.trianglesRejectedByCulling);
    EXPECT_EQ(0u, metrics.tessellation.trianglesRejectedByWindingOrDegeneracy);
    EXPECT_EQ(2u, metrics.tessellation.trianglesAfterCulling);
    EXPECT_EQ(2u, metrics.tessellation.trianglesAfterClipping);

    EXPECT_EQ(4u, metrics.tiling.tileCount);
    EXPECT_GT(metrics.tiling.nonEmptyTileCount, 0u);
    EXPECT_GE(metrics.tiling.triangleReferences, 2u);
    EXPECT_GT(metrics.tiling.maxTriangleReferencesPerTile, 0u);
    EXPECT_GE(metrics.tiling.p95TriangleReferencesPerTile, 1.0);

    EXPECT_FALSE(metrics.scheduling.automaticQueueSize);
    EXPECT_EQ(4u, metrics.scheduling.configuredQueueSize);
    EXPECT_EQ(4u, metrics.scheduling.resolvedQueueSize);
    ASSERT_EQ(1u, metrics.scheduling.evaluatedQueueSizes.size());
    EXPECT_EQ(4u, metrics.scheduling.evaluatedQueueSizes.front());
    EXPECT_EQ("explicit_queue_size", metrics.scheduling.decision);
    EXPECT_EQ("caller_override", metrics.scheduling.reason);

    EXPECT_GT(metrics.fragments.coveredSamples, 0u);
    EXPECT_EQ(metrics.fragments.coveredSamples, metrics.fragments.stencilTests);
    EXPECT_EQ(metrics.fragments.coveredSamples, metrics.fragments.depthTests);
    EXPECT_GT(metrics.fragments.depthFails, 0u);
    EXPECT_EQ(metrics.fragments.depthTests,
              metrics.fragments.depthPasses + metrics.fragments.depthFails);
    EXPECT_EQ(metrics.fragments.depthPasses, metrics.fragments.shadedFragments);
    EXPECT_EQ(metrics.fragments.depthPasses, metrics.fragments.colorWrites);
    EXPECT_EQ(0u, metrics.fragments.stencilFails);
    EXPECT_EQ(0u, metrics.fragments.alphaTestFails);

    EXPECT_EQ(2u, metrics.diagnosticImages.coverage.max);
    EXPECT_EQ(2u, metrics.diagnosticImages.depthTest.max);
    EXPECT_EQ(1u, metrics.diagnosticImages.depthPass.max);
    EXPECT_EQ(1u, metrics.diagnosticImages.shade.max);
    EXPECT_EQ(1u, metrics.diagnosticImages.colorWrite.max);
    EXPECT_GT(metrics.timings.totalRenderSeconds, 0.0);
  }

  TEST(Rasterizer, PreviewQualitySelectsLowerLodForSmallProjectedPrimitives) {
    std::vector<int> requestedLods;
    auto primitive = std::make_shared<LodRecordingPrimitive>(&requestedLods);
    auto scene = std::make_shared<Scene>(Colord::white());
    auto instance = std::make_shared<Instance>(primitive);
    instance->setMatrix(Matrix4d::translate(0.0, 0.0, 60.0));
    scene->add(instance);

    Rasterizer preview(standardCamera(), scene);
    preview.setLod(3);
    preview.setTessellationQuality(Rasterizer::TessellationQuality::Preview);
    Buffer<Colord> previewColor(64, 64);
    preview.render(previewColor);

    ASSERT_EQ(1u, requestedLods.size());
    EXPECT_LT(requestedLods.back(), 3);
    EXPECT_GT(preview.lastMetrics().tessellation.screenSpaceLodReductions, 0u);
    EXPECT_GT(preview.lastMetrics().tessellation.maxProjectedPrimitivePixels, 0.0);

    requestedLods.clear();
    Rasterizer final(standardCamera(), scene);
    final.setLod(3);
    final.setTessellationQuality(Rasterizer::TessellationQuality::Final);
    Buffer<Colord> finalColor(64, 64);
    final.render(finalColor);

    ASSERT_EQ(1u, requestedLods.size());
    EXPECT_EQ(3, requestedLods.back());
    EXPECT_EQ(0u, final.lastMetrics().tessellation.screenSpaceLodReductions);
  }

  TEST(Rasterizer, ScreenSpaceErrorOverrideCanForceFinalRenderReduction) {
    std::vector<int> requestedLods;
    auto primitive = std::make_shared<LodRecordingPrimitive>(&requestedLods);
    auto scene = std::make_shared<Scene>(Colord::white());
    auto instance = std::make_shared<Instance>(primitive);
    instance->setMatrix(Matrix4d::translate(0.0, 0.0, 60.0));
    scene->add(instance);

    Rasterizer engine(standardCamera(), scene);
    engine.setLod(3);
    engine.setTessellationQuality(Rasterizer::TessellationQuality::Final);
    engine.setMaximumScreenSpaceError(8.0);
    Buffer<Colord> color(64, 64);
    engine.render(color);

    ASSERT_EQ(1u, requestedLods.size());
    EXPECT_LT(requestedLods.back(), 3);
  }

  TEST(Rasterizer, RepeatedSourcePartInstancesReuseLodVariantCache) {
    std::vector<int> requestedLods;
    auto primitive = std::make_shared<LodRecordingPrimitive>(&requestedLods);
    auto scene = std::make_shared<Scene>(Colord::white());
    for (double x : {-0.2, 0.2}) {
      auto instance = std::make_shared<Instance>(primitive);
      instance->setMatrix(Matrix4d::translate(x, 0.0, 60.0));
      scene->add(instance);
    }

    Rasterizer engine(standardCamera(), scene);
    engine.setLod(3);
    engine.setTessellationQuality(Rasterizer::TessellationQuality::Preview);
    Buffer<Colord> color(64, 64);
    engine.render(color);

    EXPECT_EQ(1u, requestedLods.size());
    EXPECT_EQ(1u, engine.lastMetrics().tessellation.lodVariantCacheMisses);
    EXPECT_EQ(1u, engine.lastMetrics().tessellation.lodVariantCacheHits);
  }

  TEST(Rasterizer, ConservativeDepthOcclusionReducesCoverageAndDepthTestsOnDenseOcclusion) {
    auto scene = sceneWithFrontOccluderAndDenseBackLayer(24);
    Rasterizer occluding(standardCamera(), scene);
    occluding.setMaximumThreads(2);
    occluding.setQueueSize(4);
    occluding.setCullMode(Rasterizer::CullMode::Front);

    Buffer<Colord> occludingColor(64, 64);
    occluding.render(occludingColor);

    Rasterizer fallback(standardCamera(), scene);
    fallback.setMaximumThreads(2);
    fallback.setQueueSize(4);
    fallback.setCullMode(Rasterizer::CullMode::Front);
    fallback.setAlphaTestEnabled(true);
    fallback.setAlphaFunc(Rasterizer::AlphaFunc::Always, 0.0);

    Buffer<Colord> fallbackColor(64, 64);
    fallback.render(fallbackColor);

    expectBuffersEqual(fallbackColor, occludingColor);
    EXPECT_GT(occluding.lastMetrics().fragments.conservativeDepthRejectedTriangleTiles, 0u);
    EXPECT_LT(occluding.lastMetrics().fragments.coveredSamples,
              fallback.lastMetrics().fragments.coveredSamples);
    EXPECT_LT(occluding.lastMetrics().fragments.depthTests,
              fallback.lastMetrics().fragments.depthTests);
    EXPECT_EQ(0u, fallback.lastMetrics().fragments.conservativeDepthRejectedTriangleTiles);
  }

  TEST(Rasterizer, ConservativeDepthOcclusionFallsBackForTwoSidedPasses) {
    Rasterizer engine(standardCamera(), sceneWithDuplicateTriangles());
    engine.setMaximumThreads(2);
    engine.setQueueSize(4);

    Buffer<Colord> color(64, 64);
    engine.render(color);

    EXPECT_EQ(0u, engine.lastMetrics().fragments.conservativeDepthRejectedTriangleTiles);
    EXPECT_EQ(engine.lastMetrics().fragments.coveredSamples,
              engine.lastMetrics().fragments.depthTests);
  }

  TEST(Rasterizer, DepthPrepassRunsOnlyWhenExplicitlyRequestedAndEligible) {
    auto scene = sceneWithFrontOccluderAndDenseBackLayer(12);
    Rasterizer baseline(standardCamera(), scene);
    baseline.setMaximumThreads(2);
    baseline.setQueueSize(4);
    baseline.setCullMode(Rasterizer::CullMode::Front);

    Buffer<Colord> baselineColor(64, 64);
    baseline.render(baselineColor);

    Rasterizer prepass(standardCamera(), scene);
    prepass.setMaximumThreads(2);
    prepass.setQueueSize(4);
    prepass.setCullMode(Rasterizer::CullMode::Front);
    prepass.setDepthPrepassMode(Rasterizer::DepthPrepassMode::On);

    Buffer<Colord> prepassColor(64, 64);
    prepass.render(prepassColor);

    expectBuffersEqual(baselineColor, prepassColor);
    EXPECT_FALSE(baseline.lastMetrics().depthPrepass.enabled);
    EXPECT_EQ("disabled", baseline.lastMetrics().depthPrepass.decision);
    EXPECT_TRUE(prepass.lastMetrics().depthPrepass.enabled);
    EXPECT_EQ("on", prepass.lastMetrics().depthPrepass.requested);
    EXPECT_EQ("enabled", prepass.lastMetrics().depthPrepass.decision);
    EXPECT_GT(prepass.lastMetrics().depthPrepass.inputTriangles, 0u);
    EXPECT_GT(prepass.lastMetrics().depthPrepass.prepassSeconds, 0.0);
    EXPECT_GT(prepass.lastMetrics().depthPrepass.colorPassSeconds, 0.0);
    EXPECT_GT(prepass.lastMetrics().depthPrepass.totalMeasuredSeconds, 0.0);
  }

  TEST(Rasterizer, DepthPrepassAutoSuppressesCheapOpaquePasses) {
    Rasterizer engine(standardCamera(), sceneWithDuplicateTriangles());
    engine.setMaximumThreads(2);
    engine.setQueueSize(4);
    engine.setDepthPrepassMode(Rasterizer::DepthPrepassMode::Auto);

    Buffer<Colord> color(64, 64);
    engine.render(color);

    EXPECT_FALSE(engine.lastMetrics().depthPrepass.enabled);
    EXPECT_EQ("auto", engine.lastMetrics().depthPrepass.requested);
    EXPECT_EQ("suppressed_auto_no_expensive_shading_or_hierarchical_consumer",
              engine.lastMetrics().depthPrepass.decision);
  }

  TEST(Rasterizer, DepthPrepassSuppressesUnsupportedFixedFunctionState) {
    Rasterizer engine(standardCamera(), sceneWithDuplicateTriangles());
    engine.setMaximumThreads(2);
    engine.setQueueSize(4);
    engine.setDepthPrepassMode(Rasterizer::DepthPrepassMode::On);
    engine.setBlendingEnabled(true);

    Buffer<Colord> color(64, 64);
    engine.render(color);

    EXPECT_FALSE(engine.lastMetrics().depthPrepass.enabled);
    EXPECT_EQ("suppressed_non_opaque_or_unsupported_state",
              engine.lastMetrics().depthPrepass.decision);
  }

  TEST(Rasterizer, MetricsCaptureMaterialAndLightInputSummary) {
    auto tracked = sceneWithTrackedFrontFacingTriangle();
    tracked.scene->addLight(
      std::make_shared<DirectionalLight>(Vector3d(0.0, 0.0, -1.0), Colord::white()));
    Rasterizer engine(standardCamera(), tracked.scene);

    Buffer<Colord> color(64, 64);
    engine.render(color);

    const auto& metrics = engine.lastMetrics();
    EXPECT_EQ(1u, metrics.input.leafPrimitiveCount);
    EXPECT_EQ(1u, metrics.input.meshCount);
    EXPECT_EQ(1u, metrics.input.materialCount);
    EXPECT_EQ(1u, metrics.input.lightCount);
    EXPECT_TRUE(metrics.input.sourceKinds.empty());
  }

  TEST(Rasterizer, DiagnosticOutputBuffersIgnoreMismatchedBuffers) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    Buffer<Colord> color(64, 64);
    Buffer<double> depth(32, 32);
    depth.clear(123.0);

    Rasterizer::DiagnosticOutputBuffers outputs;
    outputs.depth = &depth;
    engine.setDiagnosticOutputBuffers(outputs);

    engine.render(color);

    EXPECT_DOUBLE_EQ(123.0, depth[16][16]);
  }

  TEST(Rasterizer, ClearDiagnosticOutputBuffersStopsWrites) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    Buffer<Colord> color(64, 64);
    Buffer<double> depth(64, 64);
    depth.clear(123.0);

    Rasterizer::DiagnosticOutputBuffers outputs;
    outputs.depth = &depth;
    engine.setDiagnosticOutputBuffers(outputs);
    engine.clearDiagnosticOutputBuffers();

    engine.render(color);

    EXPECT_DOUBLE_EQ(123.0, depth[32][32]);
  }

  TEST(Rasterizer, ColorLoadOpLoadPreservesExistingFramebufferWhenNothingDraws) {
    const Colord loadedColor(0.1, 0.2, 0.9);
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setColorLoadOp(Rasterizer::AttachmentLoadOp::Load);
    engine.setDepthFunc(Rasterizer::DepthFunc::Never);
    Buffer<Colord> buffer(64, 64);
    buffer.clear(loadedColor);

    engine.render(buffer);

    EXPECT_EQ(loadedColor, buffer[0][0]);
    EXPECT_EQ(loadedColor, buffer[32][32]);
  }

  TEST(Rasterizer, ColorLoadOpLoadPreservesExistingFramebufferThroughTiledMSAA) {
    const Colord loadedColor(0.1, 0.2, 0.9);
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setColorLoadOp(Rasterizer::AttachmentLoadOp::Load);
    engine.setDepthFunc(Rasterizer::DepthFunc::Never);
    engine.setMSAASamples(4);
    engine.setQueueSize(4);
    Buffer<Colord> buffer(64, 64);
    buffer.clear(loadedColor);

    engine.render(buffer);

    EXPECT_EQ(loadedColor, buffer[0][0]);
    EXPECT_EQ(loadedColor, buffer[32][32]);
  }

  TEST(Rasterizer, ColorStoreOpDiscardLeavesFramebufferUnchanged) {
    const Colord loadedColor(0.1, 0.2, 0.9);
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setColorStoreOp(Rasterizer::AttachmentStoreOp::Discard);
    engine.setFragmentShader([](const Rasterizer::FragmentInput&) { return Colord::red(); });
    Buffer<Colord> buffer(64, 64);
    buffer.clear(loadedColor);

    engine.render(buffer);

    EXPECT_EQ(loadedColor, buffer[0][0]);
    EXPECT_EQ(loadedColor, buffer[32][32]);
  }

  TEST(Rasterizer, ColorLoadOpLoadPreservesDisplayFramebufferWhenNothingDraws) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setColorLoadOp(Rasterizer::AttachmentLoadOp::Load);
    engine.setDepthFunc(Rasterizer::DepthFunc::Never);
    Buffer<unsigned int> buffer(64, 64);
    buffer.clear(0x00010203u);

    engine.render(buffer);

    EXPECT_EQ(0x00010203u, buffer[0][0]);
    EXPECT_EQ(0x00010203u, buffer[32][32]);
  }

  TEST(Rasterizer, ColorStoreOpDiscardLeavesDisplayFramebufferUnchanged) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setColorStoreOp(Rasterizer::AttachmentStoreOp::Discard);
    engine.setFragmentShader([](const Rasterizer::FragmentInput&) { return Colord::red(); });
    Buffer<unsigned int> buffer(64, 64);
    buffer.clear(0x00010203u);

    engine.render(buffer);

    EXPECT_EQ(0x00010203u, buffer[0][0]);
    EXPECT_EQ(0x00010203u, buffer[32][32]);
  }

  TEST(Rasterizer, TiledDisplayFramebufferMatchesSingleTileDisplayFramebuffer) {
    Rasterizer singleTile(standardCamera(), sceneWithFrontFacingTriangle());
    singleTile.setMSAASamples(4);
    singleTile.setQueueSize(1);
    configureScreenSpaceEdgeTriangle(singleTile);

    Rasterizer tiled(standardCamera(), sceneWithFrontFacingTriangle());
    tiled.setMSAASamples(4);
    tiled.setMaximumThreads(2);
    tiled.setQueueSize(4);
    configureScreenSpaceEdgeTriangle(tiled);

    Buffer<unsigned int> expected(40, 40);
    Buffer<unsigned int> actual(40, 40);
    singleTile.render(expected);
    tiled.render(actual);

    for (int y = 0; y != expected.height(); ++y)
      for (int x = 0; x != expected.width(); ++x)
        EXPECT_EQ(expected[y][x], actual[y][x]) << "at pixel " << x << ", " << y;
  }

  TEST(Rasterizer, DepthAttachmentLoadCanRejectFragments) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setBackgroundColor(Colord::black());
    engine.setDepthLoadOp(Rasterizer::AttachmentLoadOp::Load);
    engine.setDepthStoreOp(Rasterizer::AttachmentStoreOp::Discard);

    Buffer<Colord> color(64, 64);
    Buffer<double> depth(64, 64);
    depth.clear(9.75);
    Rasterizer::AttachmentBuffers attachments;
    attachments.depth = &depth;
    engine.setAttachmentBuffers(attachments);

    engine.render(color);

    EXPECT_EQ(Colord::black(), color[32][32]);
    EXPECT_DOUBLE_EQ(9.75, depth[32][32]);
  }

  TEST(Rasterizer, DepthAttachmentStoreWritesCommittedDepth) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    Buffer<Colord> color(64, 64);
    Buffer<double> depth(64, 64);
    depth.clear(123.0);
    Rasterizer::AttachmentBuffers attachments;
    attachments.depth = &depth;
    engine.setAttachmentBuffers(attachments);

    engine.render(color);

    EXPECT_NEAR(10.0, depth[32][32], 1e-9);
    EXPECT_EQ(engine.depthClearValue(), depth[0][0]);
  }

  TEST(Rasterizer, DepthAttachmentStoreDiscardLeavesAttachmentUnchanged) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setDepthStoreOp(Rasterizer::AttachmentStoreOp::Discard);
    Buffer<Colord> color(64, 64);
    Buffer<double> depth(64, 64);
    depth.clear(123.0);
    Rasterizer::AttachmentBuffers attachments;
    attachments.depth = &depth;
    engine.setAttachmentBuffers(attachments);

    engine.render(color);

    EXPECT_DOUBLE_EQ(123.0, depth[32][32]);
  }

  TEST(Rasterizer, StencilAttachmentLoadSeedsStencilTest) {
    const Colord shaderColor(0.0, 0.5, 1.0);
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setBackgroundColor(Colord::black());
    engine.setStencilTestEnabled(true);
    engine.setStencilLoadOp(Rasterizer::AttachmentLoadOp::Load);
    engine.setStencilFunc(Rasterizer::StencilFunc::Equal, 1);
    engine.setStencilOps(Rasterizer::StencilOp::Keep, Rasterizer::StencilOp::Keep,
                         Rasterizer::StencilOp::Replace);
    engine.setFragmentShader([&](const Rasterizer::FragmentInput&) { return shaderColor; });

    Buffer<Colord> color(64, 64);
    Buffer<std::uint8_t> stencil(64, 64);
    stencil.clear(1);
    Rasterizer::AttachmentBuffers attachments;
    attachments.stencil = &stencil;
    engine.setAttachmentBuffers(attachments);

    engine.render(color);

    EXPECT_EQ(shaderColor, color[32][32]);
    EXPECT_EQ(1, stencil[0][0]);
    EXPECT_EQ(1, stencil[32][32]);
  }

  TEST(Rasterizer, StencilAttachmentStoreWritesPassingStencilValue) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setStencilTestEnabled(true);
    engine.setStencilFunc(Rasterizer::StencilFunc::Always, 7);
    engine.setStencilOps(Rasterizer::StencilOp::Keep, Rasterizer::StencilOp::Keep,
                         Rasterizer::StencilOp::Replace);

    Buffer<Colord> color(64, 64);
    Buffer<std::uint8_t> stencil(64, 64);
    stencil.clear(123);
    Rasterizer::AttachmentBuffers attachments;
    attachments.stencil = &stencil;
    engine.setAttachmentBuffers(attachments);

    engine.render(color);

    EXPECT_EQ(0, stencil[0][0]);
    EXPECT_EQ(7, stencil[32][32]);
  }

  TEST(Rasterizer, StencilAttachmentStoreDiscardLeavesAttachmentUnchanged) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setStencilTestEnabled(true);
    engine.setStencilStoreOp(Rasterizer::AttachmentStoreOp::Discard);
    engine.setStencilFunc(Rasterizer::StencilFunc::Always, 7);
    engine.setStencilOps(Rasterizer::StencilOp::Keep, Rasterizer::StencilOp::Keep,
                         Rasterizer::StencilOp::Replace);

    Buffer<Colord> color(64, 64);
    Buffer<std::uint8_t> stencil(64, 64);
    stencil.clear(123);
    Rasterizer::AttachmentBuffers attachments;
    attachments.stencil = &stencil;
    engine.setAttachmentBuffers(attachments);

    engine.render(color);

    EXPECT_EQ(123, stencil[32][32]);
  }

  TEST(Rasterizer, ShadowCascadeCountClampsToSupportedRange) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());

    engine.setShadowCascadeCount(3);
    EXPECT_EQ(3, engine.shadowCascadeCount());

    engine.setShadowCascadeCount(0);
    EXPECT_EQ(1, engine.shadowCascadeCount());

    engine.setShadowCascadeCount(9);
    EXPECT_EQ(4, engine.shadowCascadeCount());
  }

  TEST(Rasterizer, ShadowCascadeSplitLambdaClampsToSupportedRange) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());

    engine.setShadowCascadeSplitLambda(0.75);
    EXPECT_DOUBLE_EQ(0.75, engine.shadowCascadeSplitLambda());

    engine.setShadowCascadeSplitLambda(-0.25);
    EXPECT_DOUBLE_EQ(0.0, engine.shadowCascadeSplitLambda());

    engine.setShadowCascadeSplitLambda(1.25);
    EXPECT_DOUBLE_EQ(1.0, engine.shadowCascadeSplitLambda());

    engine.setShadowCascadeSplitLambda(std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(0.0, engine.shadowCascadeSplitLambda());
  }

  TEST(Rasterizer, CascadeDepthRangesBlendLinearAndLogSplits) {
    const auto linear = engine::raster::detail::cascadeDepthRanges(1.0, 100.0, 2, 0.0);
    const auto practical = engine::raster::detail::cascadeDepthRanges(1.0, 100.0, 2, 0.5);
    const auto logarithmic = engine::raster::detail::cascadeDepthRanges(1.0, 100.0, 2, 1.0);

    ASSERT_EQ(2u, linear.size());
    ASSERT_EQ(2u, practical.size());
    ASSERT_EQ(2u, logarithmic.size());
    EXPECT_DOUBLE_EQ(50.5, linear.front().second);
    EXPECT_DOUBLE_EQ(30.25, practical.front().second);
    EXPECT_DOUBLE_EQ(10.0, logarithmic.front().second);
    EXPECT_DOUBLE_EQ(practical.front().second, practical.back().first);
    EXPECT_DOUBLE_EQ(100.0, practical.back().second);
  }

  TEST(Rasterizer, ShadowFilterRadiusClampsOnlyNegativeValues) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());

    engine.setShadowFilterRadius(3);
    EXPECT_EQ(3, engine.shadowFilterRadius());

    engine.setShadowFilterRadius(-3);

    EXPECT_EQ(0, engine.shadowFilterRadius());
  }

  TEST(Rasterizer, ShadowSlopeBiasClampsOnlyNegativeValues) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());

    engine.setShadowSlopeBias(0.25);
    EXPECT_DOUBLE_EQ(0.25, engine.shadowSlopeBias());

    engine.setShadowSlopeBias(-0.25);

    EXPECT_DOUBLE_EQ(0.0, engine.shadowSlopeBias());
  }

  TEST(Rasterizer, DirectionalShadowMapExposesSamplingState) {
    const auto shadowMap = syntheticShadowMap(0.01, 0.02, 3, Rasterizer::ShadowFilterMode::PCSS);

    EXPECT_DOUBLE_EQ(0.01, shadowMap.bias());
    EXPECT_DOUBLE_EQ(0.02, shadowMap.slopeBias());
    EXPECT_EQ(3, shadowMap.filterRadius());
    EXPECT_EQ(Rasterizer::ShadowFilterMode::PCSS, shadowMap.filterMode());
  }

  TEST(Rasterizer, ShadowSlopeBiasAddsToleranceForGrazingReceivers) {
    const Vector3d receiver(0.0, 0.0, 0.0);
    const Vector3d lightDirection(0.0, 0.0, -1.0);
    const Vector3d lightFacingNormal(0.0, 0.0, -1.0);
    const Vector3d grazingNormal(0.0, 1.0, -0.1);
    auto constantOnly = syntheticShadowMap(0.01, 0.0);
    auto slopeBiased = syntheticShadowMap(0.01, 0.01);

    EXPECT_DOUBLE_EQ(0.0, constantOnly.visibility(receiver, lightFacingNormal, lightDirection));
    EXPECT_DOUBLE_EQ(0.0, slopeBiased.visibility(receiver, lightFacingNormal, lightDirection));
    EXPECT_DOUBLE_EQ(1.0, slopeBiased.visibility(receiver, grazingNormal, lightDirection));
  }

  TEST(Rasterizer, ShadowMapLookupsOutsideDepthBufferAreLit) {
    const Vector3d outsideReceiver(2.0, 0.0, 0.0);
    const Vector3d normal(0.0, 0.0, -1.0);
    const Vector3d lightDirection(0.0, 0.0, -1.0);
    auto hard = syntheticShadowMap(0.01, 0.0);
    auto pcf = syntheticShadowMap(0.01, 0.0, 1, Rasterizer::ShadowFilterMode::PCF);
    auto pcss = syntheticShadowMap(0.01, 0.0, 1, Rasterizer::ShadowFilterMode::PCSS);

    EXPECT_DOUBLE_EQ(1.0, hard.visibility(outsideReceiver, normal, lightDirection));
    EXPECT_DOUBLE_EQ(1.0, pcf.visibility(outsideReceiver, normal, lightDirection));
    EXPECT_DOUBLE_EQ(1.0, pcss.visibility(outsideReceiver, normal, lightDirection));
  }

  TEST(Rasterizer, DirectionalShadowFitUsesLightSpaceBounds) {
    const std::vector<Vector3d> points = {
      Vector3d(-1.0, -0.25, -50.0), Vector3d(1.0, -0.25, -50.0), Vector3d(-1.0, 0.25, -50.0),
      Vector3d(1.0, 0.25, -50.0),   Vector3d(-1.0, -0.25, 50.0), Vector3d(1.0, -0.25, 50.0),
      Vector3d(-1.0, 0.25, 50.0),   Vector3d(1.0, 0.25, 50.0),
    };
    const auto fit = engine::raster::detail::directionalShadowFitForPoints(
      points, Vector3d(0.0, 0.0, -1.0), 0.1, 64);
    engine::raster::detail::DirectionalShadowCamera shadowCamera(fit);
    shadowCamera.setViewPlane(std::make_shared<ViewPlane>());
    shadowCamera.viewPlane()->setup(Matrix4d(), Recti(64, 64));

    EXPECT_LT(fit.halfExtent, 2.0);
    EXPECT_EQ(fit.origin, shadowCamera.origin());
    EXPECT_EQ(fit.basis.forward, shadowCamera.forward());
    EXPECT_EQ(fit.basis.right, shadowCamera.right());
    EXPECT_EQ(fit.basis.up, shadowCamera.up());
    EXPECT_EQ(fit.halfExtent, shadowCamera.halfExtent());
    for (const Vector3d& point : points) {
      const Vector4d clip = shadowCamera.projectPointToClipSpace(point);
      EXPECT_LE(std::abs(clip.x()), 1.0);
      EXPECT_LE(std::abs(clip.y()), 1.0);
      EXPECT_GE(clip.z(), 0.1);
    }
  }

  TEST(Rasterizer, ShadowMapBuilderBuildsDirectionalShadowMaps) {
    auto cam = std::make_shared<PinholeCamera>(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.5));
    auto scene = sceneWithDirectionalShadowCaster();
    Rasterizer engine(cam, scene);
    engine.setShadowMapsEnabled(true);
    engine.setShadowMapSize(64);
    engine.setShadowBias(0.1);
    QThreadPool threadPool;
    threadPool.setMaxThreadCount(1);
    std::atomic<bool> cancelled(false);

    const auto shadowMaps =
      engine::raster::detail::RasterShadowMapBuilder(engine, scene, cam, threadPool, cancelled)
        .build();

    ASSERT_FALSE(shadowMaps.empty());
    ASSERT_EQ(1u, shadowMaps.directionalMaps().size());
    ASSERT_FALSE(scene->lights().empty());
    const auto* shadowMap = shadowMaps.forLight(scene->lights().front().get());
    ASSERT_NE(nullptr, shadowMap);
    EXPECT_EQ(shadowMap, &shadowMaps.directionalMaps().front());
    EXPECT_LT(shadowMap->visibility(Vector3d(0.6, 0.0, 1.0), Vector3d(0.0, 0.0, -1.0),
                                    Vector3d(-0.5, 0.2, -1.0)),
              1.0);
  }

  TEST(Rasterizer, ShadowMapBuilderRendersFirstDirectionalDepthPreview) {
    auto cam = std::make_shared<PinholeCamera>(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.5));
    auto scene = sceneWithDirectionalShadowCaster();
    Rasterizer engine(cam, scene);
    engine.setShadowMapsEnabled(true);
    engine.setShadowMapSize(64);
    engine.setShadowBias(0.1);
    QThreadPool threadPool;
    threadPool.setMaxThreadCount(1);
    std::atomic<bool> cancelled(false);
    Buffer<double> depth(64, 64);

    const bool rendered =
      engine::raster::detail::RasterShadowMapBuilder(engine, scene, cam, threadPool, cancelled)
        .renderFirstDirectionalDepth(depth);

    EXPECT_TRUE(rendered);
    EXPECT_GT(countFiniteDepths(depth), 0);
  }

  TEST(Rasterizer, ClipDepthsClampToValidRange) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());

    engine.setNearClipDepth(-1.0);
    EXPECT_GT(engine.nearClipDepth(), 0.0);

    engine.setFarClipDepth(engine.nearClipDepth() * 0.5);
    EXPECT_GT(engine.farClipDepth(), engine.nearClipDepth());

    engine.clearFarClipDepth();
    EXPECT_TRUE(std::isinf(engine.farClipDepth()));
  }

  TEST(Rasterizer, NearClipDepthCanClipGeometryBeforeRasterization) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setBackgroundColor(Colord::black());
    engine.setNearClipDepth(100.0);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(0, countPixelsNotEqualTo(buffer, Colord::black()));
  }

  TEST(Rasterizer, FarClipDepthCanClipGeometryBeforeRasterization) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setBackgroundColor(Colord::black());
    engine.setFarClipDepth(1.0);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(0, countPixelsNotEqualTo(buffer, Colord::black()));
  }

  TEST(Rasterizer, DepthFuncNeverRejectsFragments) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setBackgroundColor(Colord::black());
    engine.setDepthFunc(Rasterizer::DepthFunc::Never);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(0, countPixelsNotEqualTo(buffer, Colord::black()));
  }

  TEST(Rasterizer, PositiveDepthBiasPushesFragmentsFartherBeforeDepthTest) {
    const Colord shaderColor(1.0, 0.0, 0.0);
    auto scene = sceneWithFrontFacingTriangle();

    Rasterizer unbiased(standardCamera(), scene);
    unbiased.setBackgroundColor(Colord::black());
    unbiased.setDepthClearValue(10.5);
    unbiased.setFragmentShader([&](const Rasterizer::FragmentInput&) { return shaderColor; });

    Rasterizer biased(standardCamera(), scene);
    biased.setBackgroundColor(Colord::black());
    biased.setDepthClearValue(10.5);
    biased.setDepthBias(1.0);
    biased.setFragmentShader([&](const Rasterizer::FragmentInput&) { return shaderColor; });

    Buffer<Colord> unbiasedBuffer(64, 64);
    Buffer<Colord> biasedBuffer(64, 64);
    unbiased.render(unbiasedBuffer);
    biased.render(biasedBuffer);

    EXPECT_EQ(shaderColor, unbiasedBuffer[32][32]);
    EXPECT_EQ(Colord::black(), biasedBuffer[32][32]);
  }

  TEST(Rasterizer, NegativeDepthBiasPullsFragmentsForwardBeforeDepthTest) {
    const Colord shaderColor(0.0, 1.0, 0.0);
    auto scene = sceneWithFrontFacingTriangle();

    Rasterizer unbiased(standardCamera(), scene);
    unbiased.setBackgroundColor(Colord::black());
    unbiased.setDepthClearValue(9.75);
    unbiased.setFragmentShader([&](const Rasterizer::FragmentInput&) { return shaderColor; });

    Rasterizer biased(standardCamera(), scene);
    biased.setBackgroundColor(Colord::black());
    biased.setDepthClearValue(9.75);
    biased.setDepthBias(-0.5);
    biased.setFragmentShader([&](const Rasterizer::FragmentInput&) { return shaderColor; });

    Buffer<Colord> unbiasedBuffer(64, 64);
    Buffer<Colord> biasedBuffer(64, 64);
    unbiased.render(unbiasedBuffer);
    biased.render(biasedBuffer);

    EXPECT_EQ(Colord::black(), unbiasedBuffer[32][32]);
    EXPECT_EQ(shaderColor, biasedBuffer[32][32]);
  }

  TEST(Rasterizer, DiagnosticDepthOutputIncludesDepthBias) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setDepthBias(0.25);
    Buffer<Colord> color(64, 64);
    Buffer<double> depth(64, 64);
    Rasterizer::DiagnosticOutputBuffers outputs;
    outputs.depth = &depth;
    engine.setDiagnosticOutputBuffers(outputs);

    engine.render(color);

    EXPECT_NEAR(10.25, depth[32][32], 1e-9);
  }

  TEST(Rasterizer, DisabledDepthWritesLetLaterGeometryOverdraw) {
    const Colord nearColor(1.0, 0.0, 0.0);
    const Colord farColor(0.0, 1.0, 0.0);

    Rasterizer defaultDepth(standardCamera(), sceneWithOverlappingTriangles());
    defaultDepth.setFragmentShader([&](const Rasterizer::FragmentInput& fragment) {
      return fragment.faceIdx == 0 ? nearColor : farColor;
    });

    Rasterizer noDepthWrites(standardCamera(), sceneWithOverlappingTriangles());
    noDepthWrites.setDepthWriteEnabled(false);
    noDepthWrites.setFragmentShader([&](const Rasterizer::FragmentInput& fragment) {
      return fragment.faceIdx == 0 ? nearColor : farColor;
    });

    Buffer<Colord> defaultBuffer(64, 64);
    Buffer<Colord> noWriteBuffer(64, 64);
    defaultDepth.render(defaultBuffer);
    noDepthWrites.render(noWriteBuffer);

    EXPECT_EQ(nearColor, defaultBuffer[32][32]);
    EXPECT_EQ(farColor, noWriteBuffer[32][32]);
  }

  TEST(Rasterizer, StencilFailOpCanSeedLaterGeometry) {
    const Colord secondTriangleColor(0.0, 0.5, 1.0);

    Rasterizer engine(standardCamera(), sceneWithDuplicateTriangles());
    engine.setStencilTestEnabled(true);
    engine.setStencilFunc(Rasterizer::StencilFunc::Equal, 1);
    engine.setStencilOps(Rasterizer::StencilOp::Replace, Rasterizer::StencilOp::Keep,
                         Rasterizer::StencilOp::Keep);
    engine.setFragmentShader([&](const Rasterizer::FragmentInput&) { return secondTriangleColor; });

    Buffer<Colord> buffer(64, 64);
    engine.render(buffer);

    EXPECT_EQ(secondTriangleColor, buffer[32][32]);
  }

  TEST(Rasterizer, SharedTriangleEdgeDoesNotDoubleApplyStencil) {
    const Colord overlapColor(1.0, 0.0, 0.0);

    Rasterizer engine(standardCamera(), sceneWithAdjacentQuadTriangles());
    engine.setBackgroundColor(Colord::black());
    engine.setDepthFunc(Rasterizer::DepthFunc::Always);
    engine.setStencilTestEnabled(true);
    engine.setStencilFunc(Rasterizer::StencilFunc::Equal, 1);
    engine.setStencilOps(Rasterizer::StencilOp::Replace, Rasterizer::StencilOp::Keep,
                         Rasterizer::StencilOp::Keep);
    configureScreenSpaceQuad(engine);
    engine.setFragmentShader([&](const Rasterizer::FragmentInput&) { return overlapColor; });

    Buffer<Colord> buffer(32, 32);
    engine.render(buffer);

    EXPECT_EQ(0, countPixels(buffer, overlapColor));
  }

  TEST(Rasterizer, StencilEnabledBuiltInFragmentMatchesDefaultWhenAlwaysPasses) {
    Rasterizer fixedPipeline(standardCamera(), sceneWithFrontFacingTriangle());
    Rasterizer stencilPipeline(standardCamera(), sceneWithFrontFacingTriangle());
    stencilPipeline.setStencilTestEnabled(true);

    Buffer<Colord> fixedBuffer(64, 64);
    Buffer<Colord> stencilBuffer(64, 64);
    fixedPipeline.render(fixedBuffer);
    stencilPipeline.render(stencilBuffer);

    expectBuffersEqual(fixedBuffer, stencilBuffer);
  }

  TEST(Rasterizer, ColorWriteMaskPreservesDisabledChannels) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setBackgroundColor(Colord(0.1, 0.2, 0.3));
    engine.setColorWriteMask(false, true, false);
    engine.setFragmentShader(
      [](const Rasterizer::FragmentInput&) { return Colord(0.8, 0.9, 1.0); });

    Buffer<Colord> buffer(64, 64);
    engine.render(buffer);

    EXPECT_COLOR_NEAR(Colord(0.1, 0.9, 0.3), buffer[32][32], 1e-12);
  }

  TEST(Rasterizer, BlendStateCombinesSourceAndDestinationColor) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setBackgroundColor(Colord(0.2, 0.4, 0.6));
    engine.setBlendingEnabled(true);
    engine.setBlendFactors(Rasterizer::BlendFactor::ConstantAlpha,
                           Rasterizer::BlendFactor::OneMinusConstantAlpha);
    engine.setBlendConstantAlpha(0.25);
    engine.setFragmentShader(
      [](const Rasterizer::FragmentInput&) { return Colord(1.0, 0.0, 0.0); });

    Buffer<Colord> buffer(64, 64);
    engine.render(buffer);

    EXPECT_COLOR_NEAR(Colord(0.4, 0.3, 0.45), buffer[32][32], 1e-12);
  }

  TEST(Rasterizer, SourceAlphaBlendUsesTextureSourcedAlpha) {
    auto scene = sceneWithTexturedFrontFacingTriangle(
      std::make_shared<ConstantColorTexture>(Colord(0.5, 0.0, 0.0)));
    Rasterizer engine(standardCamera(), scene);
    engine.setBackgroundColor(Colord(0.2, 0.4, 0.6));
    engine.setBlendingEnabled(true);
    engine.setBlendFactors(Rasterizer::BlendFactor::SourceAlpha,
                           Rasterizer::BlendFactor::OneMinusSourceAlpha);

    Buffer<Colord> buffer(64, 64);
    engine.render(buffer);

    EXPECT_COLOR_NEAR(Colord(0.35, 0.2, 0.3), buffer[32][32], 1e-12);
  }

  TEST(Rasterizer, SourceAlphaBlendUsesTransparentMaterialOpacity) {
    auto material =
      std::make_shared<TransparentMaterial>(std::make_shared<ConstantColorTexture>(Colord::red()));
    material->setTransmissionCoefficient(0.75);
    auto scene = sceneWithMaterialFrontFacingTriangle(material);
    scene->setAmbient(Colord::white());
    Rasterizer engine(standardCamera(), scene);
    engine.setBackgroundColor(Colord(0.2, 0.4, 0.6));
    engine.setBlendingEnabled(true);
    engine.setBlendFactors(Rasterizer::BlendFactor::SourceAlpha,
                           Rasterizer::BlendFactor::OneMinusSourceAlpha);

    Buffer<Colord> buffer(64, 64);
    engine.render(buffer);

    EXPECT_COLOR_NEAR(Colord(0.4, 0.3, 0.45), buffer[32][32], 1e-12);
  }

  TEST(Rasterizer, AlphaTestFailureSkipsColorAndDepthWrites) {
    auto scene = sceneWithTexturedFrontFacingTriangle(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.0, 0.0)));
    Rasterizer engine(standardCamera(), scene);
    engine.setBackgroundColor(Colord::black());
    engine.setAlphaTestEnabled(true);
    engine.setAlphaFunc(Rasterizer::AlphaFunc::Greater, 0.5);

    Buffer<Colord> color(64, 64);
    Buffer<double> depth(64, 64);
    Rasterizer::AttachmentBuffers attachments;
    attachments.depth = &depth;
    engine.setAttachmentBuffers(attachments);

    engine.render(color);

    EXPECT_EQ(Colord::black(), color[32][32]);
    EXPECT_EQ(engine.depthClearValue(), depth[32][32]);
  }

  TEST(Rasterizer, AlphaTestPassWritesColorAndDepth) {
    auto scene = sceneWithTexturedFrontFacingTriangle(
      std::make_shared<ConstantColorTexture>(Colord(0.75, 0.0, 0.0)));
    Rasterizer engine(standardCamera(), scene);
    engine.setBackgroundColor(Colord::black());
    engine.setAlphaTestEnabled(true);
    engine.setAlphaFunc(Rasterizer::AlphaFunc::Greater, 0.5);

    Buffer<Colord> color(64, 64);
    Buffer<double> depth(64, 64);
    Rasterizer::AttachmentBuffers attachments;
    attachments.depth = &depth;
    engine.setAttachmentBuffers(attachments);

    engine.render(color);

    EXPECT_EQ(Colord(0.75, 0.0, 0.0), color[32][32]);
    EXPECT_NEAR(10.0, depth[32][32], 1e-9);
  }

  TEST(Rasterizer, TiledRenderMatchesSingleTileRenderWithBlendState) {
    auto scene = sceneWithFrontFacingTriangle();
    auto configure = [](Rasterizer& engine) {
      engine.setBackgroundColor(Colord(0.2, 0.4, 0.6));
      engine.setBlendingEnabled(true);
      engine.setBlendFactors(Rasterizer::BlendFactor::ConstantAlpha,
                             Rasterizer::BlendFactor::OneMinusConstantAlpha);
      engine.setBlendConstantAlpha(0.25);
      engine.setFragmentShader(
        [](const Rasterizer::FragmentInput&) { return Colord(1.0, 0.0, 0.0); });
    };

    Rasterizer singleTile(standardCamera(), scene);
    configure(singleTile);

    Rasterizer tiled(standardCamera(), scene);
    tiled.setQueueSize(4);
    configure(tiled);

    Buffer<Colord> singleTileBuffer(64, 64);
    Buffer<Colord> tiledBuffer(64, 64);
    singleTile.render(singleTileBuffer);
    tiled.render(tiledBuffer);

    expectBuffersEqual(singleTileBuffer, tiledBuffer);
  }

  TEST(Rasterizer, AutomaticQueueSizeSelectsTilingForModerateScreenHeavyScene) {
    Rasterizer engine(standardCamera(), sceneWithTriangleGrid(10, 8));
    engine.setMaximumThreads(4);
    engine.setFragmentShader([](const Rasterizer::FragmentInput&) { return Colord::white(); });
    Buffer<Colord> buffer(128, 128);

    engine.render(buffer);

    EXPECT_FALSE(engine.hasExplicitQueueSize());
    EXPECT_EQ(16, engine.queueSize());
    EXPECT_EQ(16, engine.lastResolvedQueueSize());
    EXPECT_TRUE(engine.lastMetrics().scheduling.automaticQueueSize);
    EXPECT_EQ(16u, engine.lastMetrics().scheduling.configuredQueueSize);
    EXPECT_EQ(16u, engine.lastMetrics().scheduling.resolvedQueueSize);
    EXPECT_EQ("tiled", engine.lastMetrics().scheduling.decision);
    EXPECT_EQ("metrics_accepted", engine.lastMetrics().scheduling.reason);
  }

  TEST(Rasterizer, AutomaticQueueSizeKeepsDenseTessellationSingleTile) {
    Rasterizer engine(standardCamera(), sceneWithTriangleGrid(80, 60));
    engine.setMaximumThreads(4);
    engine.setFragmentShader([](const Rasterizer::FragmentInput&) { return Colord::white(); });
    Buffer<Colord> buffer(128, 128);

    engine.render(buffer);

    EXPECT_FALSE(engine.hasExplicitQueueSize());
    EXPECT_EQ(16, engine.queueSize());
    EXPECT_EQ(1, engine.lastResolvedQueueSize());
    EXPECT_TRUE(engine.lastMetrics().scheduling.automaticQueueSize);
    EXPECT_EQ(16u, engine.lastMetrics().scheduling.configuredQueueSize);
    EXPECT_EQ(1u, engine.lastMetrics().scheduling.resolvedQueueSize);
    EXPECT_EQ("single_tile", engine.lastMetrics().scheduling.decision);
    EXPECT_EQ("dense_triangle_load", engine.lastMetrics().scheduling.reason);
    EXPECT_GT(engine.lastMetrics().scheduling.evaluatedQueueSizes.size(), 1u);
  }

  TEST(Rasterizer, AutomaticQueueSizeUsesCoarseTilesWhenCandidateDuplicatesReferences) {
    Rasterizer engine(standardCamera(), sceneWithRepeatedQuadrantTriangles(8));
    engine.setMaximumThreads(4);
    engine.setFragmentShader([](const Rasterizer::FragmentInput&) { return Colord::white(); });
    Buffer<Colord> buffer(128, 128);

    engine.render(buffer);

    EXPECT_FALSE(engine.hasExplicitQueueSize());
    EXPECT_EQ(16, engine.queueSize());
    EXPECT_EQ(4, engine.lastResolvedQueueSize());
    EXPECT_TRUE(engine.lastMetrics().scheduling.automaticQueueSize);
    EXPECT_EQ(4u, engine.lastMetrics().scheduling.resolvedQueueSize);
    EXPECT_EQ("tiled", engine.lastMetrics().scheduling.decision);
    EXPECT_EQ("metrics_accepted", engine.lastMetrics().scheduling.reason);
    EXPECT_GT(engine.lastMetrics().scheduling.evaluatedQueueSizes.size(), 1u);
  }

  TEST(Rasterizer, AutomaticQueueSizeKeepsLargeTriangleDuplicationSingleTile) {
    Rasterizer engine(standardCamera(), sceneWithLargeScreenTriangles());
    engine.setMaximumThreads(4);
    engine.setFragmentShader([](const Rasterizer::FragmentInput&) { return Colord::white(); });
    Buffer<Colord> buffer(128, 128);

    engine.render(buffer);

    EXPECT_EQ(1, engine.lastResolvedQueueSize());
    EXPECT_TRUE(engine.lastMetrics().scheduling.automaticQueueSize);
    EXPECT_EQ("single_tile", engine.lastMetrics().scheduling.decision);
    EXPECT_EQ("tile_reference_duplication", engine.lastMetrics().scheduling.reason);
    EXPECT_GT(engine.lastMetrics().scheduling.evaluatedQueueSizes.size(), 1u);
  }

  TEST(Rasterizer, ExplicitQueueSizeOverridesAutomaticPolicy) {
    Rasterizer forcedSingle(standardCamera(), sceneWithTriangleGrid(10, 8));
    forcedSingle.setMaximumThreads(4);
    forcedSingle.setQueueSize(1);
    forcedSingle.setFragmentShader(
      [](const Rasterizer::FragmentInput&) { return Colord::white(); });
    Buffer<Colord> singleBuffer(128, 128);

    forcedSingle.render(singleBuffer);

    EXPECT_TRUE(forcedSingle.hasExplicitQueueSize());
    EXPECT_EQ(1, forcedSingle.lastResolvedQueueSize());

    Rasterizer forcedTiled(standardCamera(), sceneWithLargeScreenTriangles());
    forcedTiled.setMaximumThreads(4);
    forcedTiled.setQueueSize(4);
    forcedTiled.setFragmentShader([](const Rasterizer::FragmentInput&) { return Colord::white(); });
    Buffer<Colord> tiledBuffer(128, 128);

    forcedTiled.render(tiledBuffer);

    EXPECT_TRUE(forcedTiled.hasExplicitQueueSize());
    EXPECT_EQ(4, forcedTiled.lastResolvedQueueSize());
  }

  TEST(Rasterizer, SetAutomaticQueueSizeRestoresDefaultPolicy) {
    Rasterizer engine(standardCamera(), sceneWithTriangleGrid(10, 8));
    engine.setMaximumThreads(4);
    engine.setQueueSize(1);
    engine.setAutomaticQueueSize();
    engine.setFragmentShader([](const Rasterizer::FragmentInput&) { return Colord::white(); });
    Buffer<Colord> buffer(128, 128);

    engine.render(buffer);

    EXPECT_FALSE(engine.hasExplicitQueueSize());
    EXPECT_EQ(16, engine.queueSize());
    EXPECT_EQ(16, engine.lastResolvedQueueSize());
  }

  TEST(Rasterizer, ClonePreservesQueueSizePolicyMode) {
    Rasterizer automatic(standardCamera(), sceneWithTriangleGrid(10, 8));
    automatic.setMaximumThreads(4);
    auto automaticClone = std::dynamic_pointer_cast<Rasterizer>(automatic.cloneForRender());

    ASSERT_TRUE(automaticClone);
    EXPECT_FALSE(automaticClone->hasExplicitQueueSize());
    EXPECT_EQ(16, automaticClone->queueSize());

    Rasterizer explicitQueue(standardCamera(), sceneWithTriangleGrid(10, 8));
    explicitQueue.setMaximumThreads(4);
    explicitQueue.setQueueSize(7);
    auto explicitClone = std::dynamic_pointer_cast<Rasterizer>(explicitQueue.cloneForRender());

    ASSERT_TRUE(explicitClone);
    EXPECT_TRUE(explicitClone->hasExplicitQueueSize());
    EXPECT_EQ(7, explicitClone->queueSize());
  }

  TEST(Rasterizer, FragmentShaderOverridesBuiltInShading) {
    const Colord shaderColor(0.25, 0.5, 0.75);
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setFragmentShader([&](const Rasterizer::FragmentInput&) { return shaderColor; });
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(shaderColor, buffer[32][32]);
  }

  TEST(Rasterizer, FragmentShaderReceivesInterpolatedUV) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setFragmentShader([&](const Rasterizer::FragmentInput& fragment) {
      return Colord(fragment.uv.x(), fragment.uv.y(), 0.0);
    });
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    expectCenterLooksLikeTriangleUV(buffer[32][32]);
  }

  TEST(Rasterizer, BuiltInMaterialTextureReceivesInterpolatedUV) {
    Rasterizer engine(standardCamera(), sceneWithTexturedFrontFacingTriangle());
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    expectCenterLooksLikeTriangleUV(buffer[32][32]);
  }

  TEST(Rasterizer, BuiltInMaterialFallbackTextureReceivesInterpolatedUV) {
    Rasterizer engine(standardCamera(),
                      sceneWithTexturedFrontFacingTriangle(std::make_shared<HitPointUVTexture>()));
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    expectCenterLooksLikeTriangleUV(buffer[32][32]);
  }

  TEST(RasterTexture, DirectUVColorTextureReadsInterpolatedUV) {
    const auto texture =
      engine::raster::detail::RasterTexture::from(std::make_shared<render::UVColorTexture>());

    const Colord color =
      texture.evaluate(nullptr, Vector3d::null, Vector3d::up(), Vector2d(0.25, 0.75));

    EXPECT_EQ(Colord(0.25, 0.75, 0.0), color);
  }

  TEST(RasterTexture, DirectUVColorTextureCanBeEvaluatedInShaderFromUV) {
    const auto texture =
      engine::raster::detail::RasterTexture::from(std::make_shared<render::UVColorTexture>());

    EXPECT_EQ(engine::raster::detail::RasterAlbedoShaderMode::UVColor,
              texture.shaderAlbedoSource().mode);
  }

  TEST(RasterTexture, DirectImageTextureCanBeEvaluatedInShaderFromImage) {
    auto image = std::make_shared<render::ImageTexture>(
      new render::UVMapping2D(2.0, 1.0), 2, 1, std::vector<Colord>{Colord::red(), Colord::green()},
      render::ImageTextureFilter::Nearest, render::ImageTextureWrap::Repeat);
    const auto texture = engine::raster::detail::RasterTexture::from(image);
    const auto source = texture.shaderAlbedoSource();

    EXPECT_EQ(engine::raster::detail::RasterAlbedoShaderMode::ImageTexture, source.mode);
    EXPECT_EQ(image.get(), source.image);
    EXPECT_EQ(2.0, source.uScale);
    EXPECT_EQ(1.0, source.vScale);
  }

  TEST(RasterTexture, TintedImageTextureCanBeEvaluatedInShaderFromImageAndTint) {
    auto image = std::make_shared<render::ImageTexture>(
      new render::UVMapping2D(2.0, 1.0), 1, 1, std::vector<Colord>{Colord::white()},
      render::ImageTextureFilter::Nearest, render::ImageTextureWrap::Repeat);
    auto tinted = std::make_shared<render::TintedTexture>(image, Colord(0.5, 0.25, 0.75));
    const auto texture = engine::raster::detail::RasterTexture::from(tinted);
    const auto source = texture.shaderAlbedoSource();

    EXPECT_EQ(engine::raster::detail::RasterAlbedoShaderMode::ImageTexture, source.mode);
    EXPECT_EQ(image.get(), source.image);
    EXPECT_EQ(Colord(0.5, 0.25, 0.75), source.tint);
    EXPECT_EQ(Colord(0.5, 0.25, 0.75),
              texture.evaluate(nullptr, Vector3d::null, Vector3d::up(), Vector2d(0.0, 0.0)));
  }

  TEST(RasterTexture, DirectUVCheckerUsesScaledUVParity) {
    auto checker = std::make_shared<render::CheckerBoardTexture>(
      new render::UVMapping2D(2.0, 4.0), std::make_shared<ConstantColorTexture>(Colord::white()),
      std::make_shared<ConstantColorTexture>(Colord::black()));
    const auto texture = engine::raster::detail::RasterTexture::from(checker);

    const Colord bright =
      texture.evaluate(nullptr, Vector3d::null, Vector3d::up(), Vector2d(0.2, 0.2));
    const Colord dark =
      texture.evaluate(nullptr, Vector3d::null, Vector3d::up(), Vector2d(0.6, 0.2));

    EXPECT_EQ(Colord::white(), bright);
    EXPECT_EQ(Colord::black(), dark);
  }

  TEST(RasterTexture, DirectUVCheckerCanBeEvaluatedInShaderFromConstants) {
    auto checker = std::make_shared<render::CheckerBoardTexture>(
      new render::UVMapping2D(2.0, 4.0), std::make_shared<ConstantColorTexture>(Colord::red()),
      std::make_shared<ConstantColorTexture>(Colord::blue()));
    const auto texture = engine::raster::detail::RasterTexture::from(checker);
    const auto source = texture.shaderAlbedoSource();

    EXPECT_EQ(engine::raster::detail::RasterAlbedoShaderMode::UVChecker, source.mode);
    EXPECT_EQ(2.0, source.uScale);
    EXPECT_EQ(4.0, source.vScale);
    EXPECT_EQ(Colord::red(), source.checkerBright);
    EXPECT_EQ(Colord::blue(), source.checkerDark);
  }

  TEST(RasterTexture, DirectPlanarCheckerUsesWorldPositionParity) {
    auto checker = std::make_shared<render::CheckerBoardTexture>(
      new render::PlanarMapping2D, std::make_shared<ConstantColorTexture>(Colord::white()),
      std::make_shared<ConstantColorTexture>(Colord::black()));
    const auto texture = engine::raster::detail::RasterTexture::from(checker);

    const Colord bright =
      texture.evaluate(nullptr, Vector3d(0.2, 0.0, 0.2), Vector3d::up(), Vector2d::null);
    const Colord dark =
      texture.evaluate(nullptr, Vector3d(1.2, 0.0, 0.2), Vector3d::up(), Vector2d::null);

    EXPECT_EQ(Colord::white(), bright);
    EXPECT_EQ(Colord::black(), dark);
  }

  TEST(RasterTexture, DirectPlanarCheckerCanBeEvaluatedInShaderFromConstants) {
    auto checker = std::make_shared<render::CheckerBoardTexture>(
      new render::PlanarMapping2D, std::make_shared<ConstantColorTexture>(Colord::red()),
      std::make_shared<ConstantColorTexture>(Colord::blue()));
    const auto texture = engine::raster::detail::RasterTexture::from(checker);
    const auto source = texture.shaderAlbedoSource();

    EXPECT_EQ(engine::raster::detail::RasterAlbedoShaderMode::PlanarChecker, source.mode);
    EXPECT_EQ(Colord::red(), source.checkerBright);
    EXPECT_EQ(Colord::blue(), source.checkerDark);
  }

  TEST(RasterTexture, DirectImageTextureUsesScaledUVMapping) {
    auto image = std::make_shared<render::ImageTexture>(
      new render::UVMapping2D(2.0, 1.0), 2, 1, std::vector<Colord>{Colord::red(), Colord::green()},
      render::ImageTextureFilter::Nearest, render::ImageTextureWrap::Repeat);
    const auto texture = engine::raster::detail::RasterTexture::from(image);

    const Colord color =
      texture.evaluate(nullptr, Vector3d::null, Vector3d::up(), Vector2d(0.6, 0.0));

    EXPECT_EQ(Colord::red(), color);
  }

  TEST(RasterTexture, DirectImageTextureUsesUVGradientsForMipSelection) {
    std::vector<Colord> pixels;
    for (int y = 0; y != 4; ++y) {
      for (int x = 0; x != 4; ++x) {
        pixels.push_back(((x + y) % 2 == 0) ? Colord::black() : Colord::white());
      }
    }
    auto image = std::make_shared<render::ImageTexture>(new render::UVMapping2D, 4, 4, pixels,
                                                        render::ImageTextureFilter::Mipmap,
                                                        render::ImageTextureWrap::Repeat);
    const auto texture = engine::raster::detail::RasterTexture::from(image);

    const Colord color =
      texture.evaluate(nullptr, Vector3d::null, Vector3d::up(), Vector2d(0.3, 0.7),
                       Vector2d(1.0, 0.0), Vector2d(0.0, 0.0));

    EXPECT_NEAR(0.5, color.r(), 1e-9);
    EXPECT_NEAR(0.5, color.g(), 1e-9);
    EXPECT_NEAR(0.5, color.b(), 1e-9);
  }

  TEST(RasterMaterial, NormalMapTransformsFromTangentSpaceToWorldSpace) {
    const auto normalMap = engine::raster::detail::RasterTexture::constant(Colord(1.0, 0.5, 1.0));
    const auto material = engine::raster::detail::RasterMaterial::constant(
      Colord::white(), 0.0, 1.0, 1.0, Colord::black(), 0.0, 16.0, normalMap, true);
    const engine::raster::detail::RasterTangentFrame frame{Vector3d::right(), Vector3d::up(), true};

    const Vector3d mapped =
      material.lightingNormal(nullptr, Vector3d::null, Vector3d::forward(), Vector2d::null,
                              Vector2d::null, Vector2d::null, frame);

    const Vector3d expected = Vector3d(1.0, 0.0, 1.0).normalized();
    EXPECT_NEAR(expected.x(), mapped.x(), 1e-9);
    EXPECT_NEAR(expected.y(), mapped.y(), 1e-9);
    EXPECT_NEAR(expected.z(), mapped.z(), 1e-9);
  }

  TEST(RasterMaterialSource, MatteMaterialHasNoRecursiveFallbackDiagnostic) {
    const auto material =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    const auto source = engine::raster::detail::RasterMaterialSource::from(material);

    EXPECT_FALSE(source.usesRecursiveFallback());
    EXPECT_EQ(engine::raster::detail::RasterMaterialSource::RecursiveFallback::None,
              source.recursiveFallback());
    EXPECT_STREQ("none", source.recursiveFallbackName());
  }

  TEST(RasterMaterialSource, ReflectiveMaterialSelectsLocalPhongFallback) {
    const auto material =
      std::make_shared<ReflectiveMaterial>(std::make_shared<ConstantColorTexture>(Colord::red()));
    const auto source = engine::raster::detail::RasterMaterialSource::from(material);

    EXPECT_TRUE(source.usesRecursiveFallback());
    EXPECT_EQ(engine::raster::detail::RasterMaterialSource::RecursiveFallback::ReflectiveLocalPhong,
              source.recursiveFallback());
    EXPECT_STREQ("reflective-local-phong", source.recursiveFallbackName());
  }

  TEST(RasterMaterialSource, TransparentMaterialSelectsAlphaPhongFallback) {
    const auto material = std::make_shared<TransparentMaterial>(
      std::make_shared<ConstantColorTexture>(Colord::white()));
    material->setTransmissionCoefficient(0.75);
    const auto source = engine::raster::detail::RasterMaterialSource::from(material);
    const auto rasterMaterial = source.forFace(0);

    EXPECT_TRUE(source.usesRecursiveFallback());
    EXPECT_EQ(
      engine::raster::detail::RasterMaterialSource::RecursiveFallback::TransparentAlphaPhong,
      source.recursiveFallback());
    EXPECT_STREQ("transparent-alpha-phong", source.recursiveFallbackName());
    EXPECT_NEAR(0.25,
                rasterMaterial.alpha(nullptr, Vector3d::null, Vector3d::forward(), Vector2d::null,
                                     Vector2d::null, Vector2d::null),
                1e-9);
  }

  TEST(RasterMaterial, NormalMapFallsBackToGeometricNormalWithoutTangentFrame) {
    const auto normalMap = engine::raster::detail::RasterTexture::constant(Colord(1.0, 0.5, 1.0));
    const auto material = engine::raster::detail::RasterMaterial::constant(
      Colord::white(), 0.0, 1.0, 1.0, Colord::black(), 0.0, 16.0, normalMap, true);

    const Vector3d mapped = material.lightingNormal(nullptr, Vector3d::null, Vector3d::forward(),
                                                    Vector2d::null, Vector2d::null, Vector2d::null,
                                                    engine::raster::detail::RasterTangentFrame{});

    EXPECT_EQ(Vector3d::forward(), mapped);
  }

  TEST(Rasterizer, BuiltInMaterialUsesNormalMappedLightingNormal) {
    auto scene = std::make_shared<Scene>(Colord::black());
    scene->setAmbient(Colord::black());
    scene->addLight(
      std::make_shared<DirectionalLight>(Vector3d(1.0, 0.0, 1.0).normalized(), Colord::white()));
    engine::raster::detail::MaterialEvaluator evaluator(scene.get(), nullptr, nullptr);
    const auto normalMap = engine::raster::detail::RasterTexture::constant(Colord(1.0, 0.5, 1.0));
    const auto material = engine::raster::detail::RasterMaterial::constant(
      Colord::white(), 0.0, 1.0, 1.0, Colord::black(), 0.0, 16.0, normalMap, true);
    const engine::raster::detail::RasterTangentFrame frame{Vector3d::right(), Vector3d::up(), true};

    const auto shaded =
      evaluator.shade(material, nullptr, Vector3d::null, Vector3d::forward(), Vector2d::null,
                      Vector2d::null, Vector2d::null, frame, 0, 0);

    EXPECT_NEAR(1.0, shaded.color.r(), 1e-9);
    EXPECT_NEAR(1.0, shaded.color.g(), 1e-9);
    EXPECT_NEAR(1.0, shaded.color.b(), 1e-9);
  }

  TEST(Rasterizer, BuiltInMaterialConstantTextureUsesStoredAlbedo) {
    const Colord albedo(0.25, 0.5, 0.75);
    auto scene =
      sceneWithTexturedFrontFacingTriangle(std::make_shared<ConstantColorTexture>(albedo));
    Rasterizer engine(standardCamera(), scene);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(albedo, buffer[32][32]);
  }

  TEST(Rasterizer, BuiltInMaterialHonorsMatteAmbientCoefficient) {
    auto material =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::red()));
    material->setAmbientCoefficient(0.25);
    auto scene = sceneWithMaterialFrontFacingTriangle(material);
    scene->setAmbient(Colord(0.4, 0.4, 0.4));
    Rasterizer engine(standardCamera(), scene);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_NEAR(0.1, buffer[32][32].r(), 0.001);
    EXPECT_NEAR(0.0, buffer[32][32].g(), 0.001);
    EXPECT_NEAR(0.0, buffer[32][32].b(), 0.001);
  }

  TEST(Rasterizer, BuiltInMaterialAddsPhongSpecularHighlight) {
    auto material =
      std::make_shared<PhongMaterial>(std::make_shared<ConstantColorTexture>(Colord::red()));
    material->setAmbientCoefficient(0.0);
    material->setDiffuseCoefficient(1.0);
    material->setSpecularColor(Colord::white());
    material->setSpecularCoefficient(0.5);
    material->setExponent(16.0);
    auto scene = sceneWithMaterialFrontFacingTriangle(material);
    scene->setAmbient(Colord::black());
    scene->addLight(std::make_shared<DirectionalLight>(Vector3d(0.0, 0.0, -1.0), Colord::white()));
    Rasterizer engine(standardCamera(), scene);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_NEAR(1.5, buffer[32][32].r(), 0.001);
    EXPECT_NEAR(0.5, buffer[32][32].g(), 0.001);
    EXPECT_NEAR(0.5, buffer[32][32].b(), 0.001);
  }

  TEST(Rasterizer, BuiltInMaterialKeepsVirtualTextureBehaviorForConstantTextureSubclasses) {
    auto scene =
      sceneWithTexturedFrontFacingTriangle(std::make_shared<OverridingConstantColorTexture>());
    Rasterizer engine(standardCamera(), scene);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    expectCenterLooksLikeTriangleUV(buffer[32][32]);
  }

  TEST(Rasterizer, BuiltInMaterialKeepsVirtualTextureBehaviorForUVColorTextureSubclasses) {
    auto scene = sceneWithTexturedFrontFacingTriangle(std::make_shared<OverridingUVColorTexture>());
    Rasterizer engine(standardCamera(), scene);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_NEAR(0.0, buffer[32][32].r(), 0.001);
    EXPECT_GT(buffer[32][32].g(), 0.35);
    EXPECT_LT(buffer[32][32].g(), 0.65);
    EXPECT_GT(buffer[32][32].b(), 0.15);
    EXPECT_LT(buffer[32][32].b(), 0.35);
  }

  TEST(Rasterizer, OrthographicProjectionInterpolatesWorldPositionLinearly) {
    auto cam = std::make_shared<OrthographicCamera>(Vector3d(0.0, 0.0, -5.0), Vector3d::null);
    Rasterizer engine(cam, sceneWithSlopedTriangle());
    engine.setFragmentShader([](const Rasterizer::FragmentInput& input) {
      return Colord(input.worldPosition.z() / 5.0, 0.0, 0.0);
    });
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_NEAR(0.5, buffer[32][32].r(), 0.03);
  }

  TEST(Rasterizer, DirectionalShadowMapsDarkenOccludedDiffuseLight) {
    auto cam = std::make_shared<PinholeCamera>(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.5));
    auto scene = sceneWithDirectionalShadowCaster();

    Rasterizer directOnly(cam, scene);
    Rasterizer shadowed(cam, scene);
    shadowed.setShadowMapsEnabled(true);
    shadowed.setShadowMapSize(256);
    shadowed.setShadowBias(0.1);

    Buffer<Colord> directBuffer(96, 96);
    Buffer<Colord> shadowBuffer(96, 96);
    directOnly.render(directBuffer);
    shadowed.render(shadowBuffer);

    const Colord directShadowPoint = colorAtWorldPoint(directBuffer, cam, Vector3d(0.6, 0.0, 1.0));
    const Colord shadowedShadowPoint =
      colorAtWorldPoint(shadowBuffer, cam, Vector3d(0.6, 0.0, 1.0));
    const Colord shadowedLitPoint = colorAtWorldPoint(shadowBuffer, cam, Vector3d(-1.2, 0.0, 1.0));

    EXPECT_GT(directShadowPoint.r(), shadowedShadowPoint.r() + 0.4);
    EXPECT_GT(shadowedLitPoint.r(), shadowedShadowPoint.r() + 0.4);
    EXPECT_LT(shadowedShadowPoint.r(), 0.35);
  }

  TEST(Rasterizer, ShadowFilterRadiusSoftensHardShadowBoundary) {
    auto cam = std::make_shared<PinholeCamera>(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.5));
    auto scene = sceneWithDirectionalShadowCaster();

    Rasterizer hard(cam, scene);
    hard.setShadowMapsEnabled(true);
    hard.setShadowMapSize(64);
    hard.setShadowBias(0.1);

    Rasterizer filtered(cam, scene);
    filtered.setShadowMapsEnabled(true);
    filtered.setShadowMapSize(64);
    filtered.setShadowBias(0.1);
    filtered.setShadowFilterRadius(2);

    Buffer<Colord> hardBuffer(96, 96);
    Buffer<Colord> filteredBuffer(96, 96);
    hard.render(hardBuffer);
    filtered.render(filteredBuffer);

    EXPECT_GT(countPixelsBrightenedByFiltering(hardBuffer, filteredBuffer), 0);
    EXPECT_GT(countPixelsDarkenedByFiltering(hardBuffer, filteredBuffer), 0);
  }

  TEST(Rasterizer, VertexShaderCanAdjustProjectedPosition) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setBackgroundColor(Colord::black());
    engine.setVertexShader([](const Rasterizer::VertexInput& vertex) {
      return Rasterizer::VertexOutput{vertex.worldPosition, vertex.normal, vertex.uv,
                                      vertex.clipPosition,
                                      vertex.screenPosition + Vector3d(1000, 0, 0)};
    });
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(0, countPixelsNotEqualTo(buffer, Colord::black()));
  }

  TEST(Rasterizer, CullModeDefaultsToBothSides) {
    Rasterizer engine(standardCamera(), sceneWithBackFacingTriangle());
    EXPECT_EQ(Rasterizer::CullMode::Both, engine.cullMode());
    EXPECT_FALSE(engine.hasCullModeOverride());

    Buffer<Colord> buffer(64, 64);
    engine.render(buffer);

    EXPECT_GT(countPixelsNotEqualTo(buffer, Colord::black()), 0);
  }

  TEST(Rasterizer, FrontSidedMaterialDefaultsToBackfaceCulling) {
    auto material = matte(Colord::white());
    material->setSidedness(render::Material::Sidedness::Front);
    auto scene = sceneWithMaterialBackFacingTriangle(material);
    scene->setAmbient(Colord::white());
    Rasterizer engine(standardCamera(), scene);
    engine.setBackgroundColor(Colord::black());
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(0, countPixelsNotEqualTo(buffer, Colord::black()));
    EXPECT_EQ(1u, engine.lastMetrics().tessellation.trianglesRejectedByCulling);
  }

  TEST(Rasterizer, UnknownMeshWindingDisablesOnlyInferredBackfaceCulling) {
    auto material = matte(Colord::white());
    material->setSidedness(render::Material::Sidedness::Front);
    auto scene =
      sceneWithMeshBackFacingTriangle(material, Mesh::FaceMetadata::WindingReliability::Unknown);
    scene->setAmbient(Colord::white());
    Rasterizer engine(standardCamera(), scene);
    engine.setBackgroundColor(Colord::black());
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_GT(countPixelsNotEqualTo(buffer, Colord::black()), 0);
    EXPECT_EQ(0u, engine.lastMetrics().tessellation.trianglesRejectedByCulling);
  }

  TEST(Rasterizer, CorrectedMeshWindingDisablesOnlyInferredBackfaceCulling) {
    auto material = matte(Colord::white());
    material->setSidedness(render::Material::Sidedness::Front);
    auto scene =
      sceneWithMeshBackFacingTriangle(material, Mesh::FaceMetadata::WindingReliability::Corrected);
    scene->setAmbient(Colord::white());
    Rasterizer engine(standardCamera(), scene);
    engine.setBackgroundColor(Colord::black());
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_GT(countPixelsNotEqualTo(buffer, Colord::black()), 0);
    EXPECT_EQ(0u, engine.lastMetrics().tessellation.trianglesRejectedByCulling);
  }

  TEST(Rasterizer, ExplicitCullModeOverridesUnreliableMeshWinding) {
    auto material = matte(Colord::white());
    material->setSidedness(render::Material::Sidedness::Front);
    auto scene =
      sceneWithMeshBackFacingTriangle(material, Mesh::FaceMetadata::WindingReliability::Unknown);
    scene->setAmbient(Colord::white());
    Rasterizer engine(standardCamera(), scene);
    engine.setBackgroundColor(Colord::black());
    engine.setCullMode(Rasterizer::CullMode::Back);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(0, countPixelsNotEqualTo(buffer, Colord::black()));
    EXPECT_EQ(1u, engine.lastMetrics().tessellation.trianglesRejectedByCulling);
  }

  TEST(Rasterizer, BackSidedMaterialDefaultsToFrontfaceCulling) {
    auto material = matte(Colord::white());
    material->setSidedness(render::Material::Sidedness::Back);
    auto scene = sceneWithMaterialFrontFacingTriangle(material);
    scene->setAmbient(Colord::white());
    Rasterizer engine(standardCamera(), scene);
    engine.setBackgroundColor(Colord::black());
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(0, countPixelsNotEqualTo(buffer, Colord::black()));
  }

  TEST(Rasterizer, TwoSidedMaterialDefaultsToNoCulling) {
    auto material = matte(Colord::white());
    material->setSidedness(render::Material::Sidedness::TwoSided);
    auto scene = sceneWithMaterialBackFacingTriangle(material);
    scene->setAmbient(Colord::white());
    Rasterizer engine(standardCamera(), scene);
    engine.setBackgroundColor(Colord::black());
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_GT(countPixelsNotEqualTo(buffer, Colord::black()), 0);
  }

  TEST(Rasterizer, ExplicitCullModeOverridesMaterialSidednessDefault) {
    auto material = matte(Colord::white());
    material->setSidedness(render::Material::Sidedness::Front);
    auto scene = sceneWithMaterialBackFacingTriangle(material);
    scene->setAmbient(Colord::white());
    Rasterizer engine(standardCamera(), scene);
    engine.setBackgroundColor(Colord::black());
    engine.setCullMode(Rasterizer::CullMode::Both);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_TRUE(engine.hasCullModeOverride());
    EXPECT_GT(countPixelsNotEqualTo(buffer, Colord::black()), 0);
  }

  TEST(Rasterizer, MetricsCountDegenerateProjectedTriangles) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setVertexShader([](const Rasterizer::VertexInput& vertex) {
      return Rasterizer::VertexOutput{vertex.worldPosition, vertex.normal, vertex.uv,
                                      vertex.clipPosition, Vector3d(32.0, 32.0, 1.0)};
    });
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(1u, engine.lastMetrics().tessellation.preparedTrianglesBeforeCulling);
    EXPECT_EQ(1u, engine.lastMetrics().tessellation.trianglesRejectedByWindingOrDegeneracy);
    EXPECT_EQ(0u, engine.lastMetrics().tessellation.trianglesAfterCulling);
    EXPECT_EQ(0u, engine.lastMetrics().tessellation.trianglesAfterClipping);
  }

  TEST(Rasterizer, BackfaceCullingSkipsBackFacingTriangles) {
    Rasterizer engine(standardCamera(), sceneWithBackFacingTriangle());
    engine.setBackgroundColor(Colord::black());
    engine.setCullMode(Rasterizer::CullMode::Back);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(0, countPixelsNotEqualTo(buffer, Colord::black()));
  }

  TEST(Rasterizer, BackfaceCullingKeepsFrontFacingTriangles) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setCullMode(Rasterizer::CullMode::Back);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_GT(countPixelsNotEqualTo(buffer, Colord::black()), 0);
  }

  TEST(Rasterizer, FrontfaceCullingSkipsFrontFacingTriangles) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setBackgroundColor(Colord::black());
    engine.setCullMode(Rasterizer::CullMode::Front);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(0, countPixelsNotEqualTo(buffer, Colord::black()));
  }

  TEST(Rasterizer, BackfaceCullingAppliesToTiledPath) {
    Rasterizer engine(standardCamera(), sceneWithBackFacingTriangle());
    engine.setBackgroundColor(Colord::black());
    engine.setCullMode(Rasterizer::CullMode::Back);
    engine.setMaximumThreads(2);
    engine.setQueueSize(4);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(0, countPixelsNotEqualTo(buffer, Colord::black()));
  }

  TEST(Rasterizer, ViewportClippedGeometryFillsFramebuffer) {
    Rasterizer engine(standardCamera(), sceneWithOversizedRectangle());
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(64 * 64, countPixelsNotEqualTo(buffer, Colord::black()));
  }

  TEST(Rasterizer, ViewportRectMapsClipSpaceIntoFramebufferSubrect) {
    Rasterizer engine(standardCamera(), sceneWithOversizedRectangle());
    engine.setBackgroundColor(Colord::black());
    engine.setViewportRect(16, 12, 32, 24);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(32 * 24, countPixelsNotEqualTo(buffer, Colord::black()));
    EXPECT_EQ(Colord::black(), buffer[20][8]);
    EXPECT_NE(Colord::black(), buffer[20][32]);
  }

  TEST(Rasterizer, ViewportRectUsesConfiguredProjectionBeforeFramebufferClipping) {
    auto cam = standardCamera();
    Rasterizer engine(cam, sceneWithOversizedRectangle());
    engine.setBackgroundColor(Colord::black());
    engine.setViewportRect(-16, -12, 32, 24);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    const Vector2d projectedCenter = cam->projectPoint(Vector3d::null);
    EXPECT_DOUBLE_EQ(0.0, projectedCenter.x());
    EXPECT_DOUBLE_EQ(0.0, projectedCenter.y());
  }

  TEST(Rasterizer, ScissorRectRejectsFragmentsOutsideSubrect) {
    Rasterizer engine(standardCamera(), sceneWithOversizedRectangle());
    engine.setBackgroundColor(Colord::black());
    engine.setScissorRect(20, 16, 24, 32);
    Buffer<Colord> buffer(64, 64);

    engine.render(buffer);

    EXPECT_EQ(24 * 32, countPixelsNotEqualTo(buffer, Colord::black()));
    EXPECT_EQ(Colord::black(), buffer[20][8]);
    EXPECT_NE(Colord::black(), buffer[20][32]);
  }

  TEST(Rasterizer, TiledRenderMatchesSingleTileRender) {
    auto cam = std::make_shared<PinholeCamera>(Vector3d(0, 0, -8), Vector3d::null);
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    scene->add(std::make_shared<Box>(Vector3d(0, 0, 10), Vector3d(5, 5, 0.1)));

    Rasterizer singleTile(cam, scene);
    singleTile.setLod(2);
    singleTile.setMaximumThreads(1);
    singleTile.setQueueSize(1);

    Rasterizer tiled(cam, scene);
    tiled.setLod(2);
    tiled.setMaximumThreads(4);
    tiled.setQueueSize(16);

    Buffer<Colord> expected(128, 128);
    Buffer<Colord> actual(128, 128);
    singleTile.render(expected);
    tiled.render(actual);

    expectBuffersEqual(expected, actual);
  }

  TEST(Rasterizer, TiledRenderMatchesSingleTileRenderWithViewportAndScissor) {
    auto scene = sceneWithOversizedRectangle();

    Rasterizer singleTile(standardCamera(), scene);
    singleTile.setViewportRect(8, 6, 48, 44);
    singleTile.setScissorRect(18, 14, 22, 24);
    singleTile.setMaximumThreads(1);
    singleTile.setQueueSize(1);

    Rasterizer tiled(standardCamera(), scene);
    tiled.setViewportRect(8, 6, 48, 44);
    tiled.setScissorRect(18, 14, 22, 24);
    tiled.setMaximumThreads(4);
    tiled.setQueueSize(16);

    Buffer<Colord> expected(64, 64);
    Buffer<Colord> actual(64, 64);
    singleTile.render(expected);
    tiled.render(actual);

    expectBuffersEqual(expected, actual);
  }

  TEST(Rasterizer, TiledRenderMatchesSingleTileRenderWithUnevenTileSizes) {
    auto cam = std::make_shared<PinholeCamera>(Vector3d(0, 0, -8), Vector3d::null);
    auto scene = std::make_shared<Scene>(Colord::white());
    scene->add(std::make_shared<Sphere>(Vector3d(0, 0, 0), 1.0));
    scene->add(std::make_shared<Box>(Vector3d(0, 0, 10), Vector3d(5, 5, 0.1)));

    Rasterizer singleTile(cam, scene);
    singleTile.setLod(2);
    singleTile.setMaximumThreads(1);
    singleTile.setQueueSize(1);

    Rasterizer tiled(cam, scene);
    tiled.setLod(2);
    tiled.setMaximumThreads(3);
    tiled.setQueueSize(6);

    Buffer<Colord> expected(127, 95);
    Buffer<Colord> actual(127, 95);
    singleTile.render(expected);
    tiled.render(actual);

    expectBuffersEqual(expected, actual);
  }

  TEST(Rasterizer, TiledRenderMatchesSingleTileRenderAcrossSharedTriangleEdge) {
    const Colord fillColor(0.2, 0.4, 0.8);

    Rasterizer singleTile(standardCamera(), sceneWithAdjacentQuadTriangles());
    configureScreenSpaceQuad(singleTile);
    singleTile.setFragmentShader([&](const Rasterizer::FragmentInput&) { return fillColor; });

    Rasterizer tiled(standardCamera(), sceneWithAdjacentQuadTriangles());
    tiled.setMaximumThreads(2);
    tiled.setQueueSize(4);
    configureScreenSpaceQuad(tiled);
    tiled.setFragmentShader([&](const Rasterizer::FragmentInput&) { return fillColor; });

    Buffer<Colord> expected(32, 32);
    Buffer<Colord> actual(32, 32);
    singleTile.render(expected);
    tiled.render(actual);

    expectBuffersEqual(expected, actual);
  }

  TEST(Rasterizer, TileBinningSkipsLargeTriangleTilesOutsideCoverage) {
    const render::TilePlan tilePlan = render::TilePlan::forBuffer(64, 64, 16);
    engine::raster::detail::RasterTriangleSet triangleSet(tilePlan);

    triangleSet.add(screenTriangle(0.0, 0.0, 64.0, 0.0, 0.0, 64.0));

    ASSERT_EQ(1u, triangleSet.triangles().size());
    EXPECT_LT(triangleSet.binnedTriangleCount(), tilePlan.size());
    EXPECT_GE(triangleSet.binnedTriangleCount(), 8u);
  }

  TEST(Rasterizer, TileBinningKeepsOrdinarySmallTriangleLocal) {
    const render::TilePlan tilePlan = render::TilePlan::forBuffer(64, 64, 16);
    engine::raster::detail::RasterTriangleSet triangleSet(tilePlan);

    triangleSet.add(screenTriangle(4.0, 4.0, 12.0, 4.0, 4.0, 12.0));

    ASSERT_EQ(1u, triangleSet.triangles().size());
    EXPECT_EQ(1u, triangleSet.binnedTriangleCount());
  }

  TEST(Rasterizer, TiledOversizedTriangleMatchesSingleTileRender) {
    Rasterizer singleTile(standardCamera(), sceneWithOversizedScreenTriangle());
    singleTile.setMaximumThreads(1);
    singleTile.setQueueSize(1);
    configureOversizedScreenTriangle(singleTile);

    Rasterizer tiled(standardCamera(), sceneWithOversizedScreenTriangle());
    tiled.setMaximumThreads(4);
    tiled.setQueueSize(16);
    configureOversizedScreenTriangle(tiled);

    Buffer<Colord> expected(64, 64);
    Buffer<Colord> actual(64, 64);
    singleTile.render(expected);
    tiled.render(actual);

    expectBuffersEqual(expected, actual);
  }

  TEST(Rasterizer, SubpixelScreenCoordinatesDoNotRoundAtHalfPixel) {
    Rasterizer beforeHalfPixel(standardCamera(), sceneWithFrontFacingTriangle());
    beforeHalfPixel.setBackgroundColor(Colord::black());
    configureScreenSpaceSubpixelTriangle(beforeHalfPixel, 18.49);

    Rasterizer afterHalfPixel(standardCamera(), sceneWithFrontFacingTriangle());
    afterHalfPixel.setBackgroundColor(Colord::black());
    configureScreenSpaceSubpixelTriangle(afterHalfPixel, 18.51);

    Buffer<Colord> before(32, 32);
    Buffer<Colord> after(32, 32);
    beforeHalfPixel.render(before);
    afterHalfPixel.render(after);

    expectBuffersEqual(before, after);
  }

  TEST(Rasterizer, SubpixelScreenCoordinatesChangeCoverageAtSamplePoint) {
    Rasterizer beforeSamplePoint(standardCamera(), sceneWithFrontFacingTriangle());
    beforeSamplePoint.setBackgroundColor(Colord::black());
    configureScreenSpaceSubpixelTriangle(beforeSamplePoint, 18.99);

    Rasterizer afterSamplePoint(standardCamera(), sceneWithFrontFacingTriangle());
    afterSamplePoint.setBackgroundColor(Colord::black());
    configureScreenSpaceSubpixelTriangle(afterSamplePoint, 19.01);

    Buffer<Colord> before(32, 32);
    Buffer<Colord> after(32, 32);
    beforeSamplePoint.render(before);
    afterSamplePoint.render(after);

    EXPECT_LT(countPixelsNotEqualTo(before, Colord::black()),
              countPixelsNotEqualTo(after, Colord::black()));
  }

  TEST(Rasterizer, TiledRenderMatchesSingleTileRenderWithSubpixelVertices) {
    Rasterizer singleTile(standardCamera(), sceneWithFrontFacingTriangle());
    configureScreenSpaceSubpixelTriangle(singleTile, 18.51);

    Rasterizer tiled(standardCamera(), sceneWithFrontFacingTriangle());
    tiled.setMaximumThreads(2);
    tiled.setQueueSize(4);
    configureScreenSpaceSubpixelTriangle(tiled, 18.51);

    Buffer<Colord> expected(32, 32);
    Buffer<Colord> actual(32, 32);
    singleTile.render(expected);
    tiled.render(actual);

    expectBuffersEqual(expected, actual);
  }

  TEST(Rasterizer, MSAAResolveBlendsPartiallyCoveredEdge) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setBackgroundColor(Colord::black());
    engine.setMSAASamples(4);
    configureScreenSpaceEdgeTriangle(engine);
    Buffer<Colord> buffer(40, 40);

    engine.render(buffer);

    EXPECT_NEAR(0.5, buffer[24][24].r(), 1e-9);
    EXPECT_NEAR(0.5, buffer[24][24].g(), 1e-9);
    EXPECT_NEAR(0.5, buffer[24][24].b(), 1e-9);
  }

  TEST(Rasterizer, MSAASampleOffsetsUseSubpixelScreenCoordinates) {
    Rasterizer engine(standardCamera(), sceneWithFrontFacingTriangle());
    engine.setBackgroundColor(Colord::black());
    engine.setMSAASamples(2);
    configureScreenSpaceMSAASubpixelTriangle(engine);
    Buffer<Colord> buffer(40, 40);

    engine.render(buffer);

    EXPECT_NEAR(0.5, buffer[16][16].r(), 1e-9);
    EXPECT_NEAR(0.5, buffer[16][16].g(), 1e-9);
    EXPECT_NEAR(0.5, buffer[16][16].b(), 1e-9);
  }

  TEST(Rasterizer, PerFragmentMSAAReusesShadedColorAcrossCoveredSamples) {
    int perSampleCalls = 0;
    Rasterizer perSample(standardCamera(), sceneWithFrontFacingTriangle());
    perSample.setBackgroundColor(Colord::black());
    perSample.setMSAASamples(4);
    configureScreenSpaceEdgeTriangle(perSample);
    perSample.setFragmentShader([&](const Rasterizer::FragmentInput&) {
      ++perSampleCalls;
      return Colord::white();
    });

    int perFragmentCalls = 0;
    Rasterizer perFragment(standardCamera(), sceneWithFrontFacingTriangle());
    perFragment.setBackgroundColor(Colord::black());
    perFragment.setMSAASamples(4);
    perFragment.setMSAAShadingMode(Rasterizer::MSAAShadingMode::PerFragment);
    configureScreenSpaceEdgeTriangle(perFragment);
    perFragment.setFragmentShader([&](const Rasterizer::FragmentInput&) {
      ++perFragmentCalls;
      return Colord::white();
    });

    Buffer<Colord> perSampleBuffer(40, 40);
    Buffer<Colord> perFragmentBuffer(40, 40);
    perSample.render(perSampleBuffer);
    perFragment.render(perFragmentBuffer);

    expectBuffersEqual(perSampleBuffer, perFragmentBuffer);
    EXPECT_GT(perSampleCalls, 0);
    EXPECT_GT(perFragmentCalls, 0);
    EXPECT_LT(perFragmentCalls, perSampleCalls);
  }

  TEST(Rasterizer, TiledMSAAMatchesSingleTileMSAA) {
    Rasterizer singleTile(standardCamera(), sceneWithFrontFacingTriangle());
    singleTile.setMSAASamples(4);
    configureScreenSpaceEdgeTriangle(singleTile);

    Rasterizer tiled(standardCamera(), sceneWithFrontFacingTriangle());
    tiled.setMSAASamples(4);
    tiled.setMaximumThreads(2);
    tiled.setQueueSize(4);
    configureScreenSpaceEdgeTriangle(tiled);

    Buffer<Colord> expected(40, 40);
    Buffer<Colord> actual(40, 40);
    singleTile.render(expected);
    tiled.render(actual);

    expectBuffersEqual(expected, actual);
  }

  TEST(Rasterizer, TiledMSAAReusesTileScratchAcrossFrames) {
    Rasterizer singleTile(standardCamera(), sceneWithFrontFacingTriangle());
    singleTile.setMSAASamples(4);
    configureScreenSpaceEdgeTriangle(singleTile);

    Rasterizer tiled(standardCamera(), sceneWithFrontFacingTriangle());
    tiled.setMSAASamples(4);
    tiled.setMaximumThreads(2);
    tiled.setQueueSize(4);
    configureScreenSpaceEdgeTriangle(tiled);

    Buffer<Colord> expected(40, 40);
    Buffer<Colord> first(40, 40);
    Buffer<Colord> second(40, 40);

    engine::raster::detail::resetMSAATileScratchAllocationCount();
    singleTile.render(expected);
    tiled.render(first);
    const std::size_t firstScratchAllocations =
      engine::raster::detail::msaaTileScratchAllocationCount();

    engine::raster::detail::resetMSAATileScratchAllocationCount();
    tiled.render(second);
    const std::size_t secondScratchAllocations =
      engine::raster::detail::msaaTileScratchAllocationCount();

    expectBuffersEqual(expected, first);
    expectBuffersEqual(expected, second);
    EXPECT_GT(firstScratchAllocations, 0u);
    EXPECT_EQ(0u, secondScratchAllocations);
  }

  TEST(Rasterizer, TiledPerFragmentMSAAMatchesSingleTileMSAAWithUnevenTileSizes) {
    Rasterizer singleTile(standardCamera(), sceneWithFrontFacingTriangle());
    singleTile.setMSAASamples(4);
    singleTile.setMSAAShadingMode(Rasterizer::MSAAShadingMode::PerFragment);
    configureScreenSpaceEdgeTriangle(singleTile);

    Rasterizer tiled(standardCamera(), sceneWithFrontFacingTriangle());
    tiled.setMSAASamples(4);
    tiled.setMSAAShadingMode(Rasterizer::MSAAShadingMode::PerFragment);
    tiled.setMaximumThreads(3);
    tiled.setQueueSize(6);
    configureScreenSpaceEdgeTriangle(tiled);

    Buffer<Colord> expected(41, 37);
    Buffer<Colord> actual(41, 37);
    singleTile.render(expected);
    tiled.render(actual);

    expectBuffersEqual(expected, actual);
  }

  TEST(Rasterizer, TiledMSAAMatchesSingleTileMSAAWithStencil) {
    const Colord secondTriangleColor(0.0, 0.5, 1.0);

    Rasterizer singleTile(standardCamera(), sceneWithDuplicateTriangles());
    singleTile.setMSAASamples(4);
    singleTile.setStencilTestEnabled(true);
    singleTile.setStencilFunc(Rasterizer::StencilFunc::Equal, 1);
    singleTile.setStencilOps(Rasterizer::StencilOp::Replace, Rasterizer::StencilOp::Keep,
                             Rasterizer::StencilOp::Keep);
    singleTile.setFragmentShader(
      [&](const Rasterizer::FragmentInput&) { return secondTriangleColor; });

    Rasterizer tiled(standardCamera(), sceneWithDuplicateTriangles());
    tiled.setMSAASamples(4);
    tiled.setMaximumThreads(2);
    tiled.setQueueSize(4);
    tiled.setStencilTestEnabled(true);
    tiled.setStencilFunc(Rasterizer::StencilFunc::Equal, 1);
    tiled.setStencilOps(Rasterizer::StencilOp::Replace, Rasterizer::StencilOp::Keep,
                        Rasterizer::StencilOp::Keep);
    tiled.setFragmentShader([&](const Rasterizer::FragmentInput&) { return secondTriangleColor; });

    Buffer<Colord> expected(64, 64);
    Buffer<Colord> actual(64, 64);
    singleTile.render(expected);
    tiled.render(actual);

    expectBuffersEqual(expected, actual);
  }

  TEST(Rasterizer, UncancelAllowsSubsequentRender) {
    Rasterizer engine(angledCamera(), sceneWithBox());
    Buffer<Colord> buffer(64, 64);

    engine.cancel();
    engine.uncancel();
    engine.render(buffer);

    EXPECT_GT(countPixelsNotEqualTo(buffer, Colord::black()), 0);
  }
}
