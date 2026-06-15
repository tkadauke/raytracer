#include <gtest/gtest.h>

#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/RenderGraphCompiler.h"
#include "engine/graph/RenderGraphExecutionTrace.h"
#include "engine/graph/RenderGraphTypes.h"
#include "render/IntersectionSceneCompiler.h"
#include "world/objects/Camera.h"
#include "world/objects/DirectionalLight.h"
#include "world/objects/Disk.h"
#include "world/objects/OpenCylinder.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/PointLight.h"
#include "world/objects/Rectangle.h"
#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"
#include "world/objects/Torus.h"

#include "core/Buffer.h"

#include <QJsonObject>
#include <QString>

#include <memory>

namespace TracingExecutionInspectionSceneTest {
  namespace {
    constexpr const char* scenePath = "scenes/tracing_execution_inspection_demo.json";
    constexpr const char* cameraId = "{68500000-0000-0000-0000-000000000001}";

    std::unique_ptr<Scene> loadScene() {
      auto scene = std::make_unique<Scene>();
      if (!scene->load(QString::fromUtf8(scenePath))) {
        return nullptr;
      }
      return scene;
    }

    template<class T>
    T* requireElement(Scene& scene, const char* id) {
      auto* element = qobject_cast<T*>(scene.findById(QString::fromUtf8(id)));
      EXPECT_NE(nullptr, element) << id;
      return element;
    }
  }

  TEST(TracingExecutionInspectionScene, LoadsSupportedSubsetForModeler) {
    auto scene = loadScene();

    ASSERT_NE(nullptr, scene);
    EXPECT_EQ(QStringLiteral("Tracing Execution Inspection Demo"), scene->name());
    EXPECT_TRUE(scene->hasRenderIntent());
    EXPECT_NE(nullptr, requireElement<PinholeCamera>(*scene, cameraId));
    EXPECT_NE(nullptr, requireElement<PointLight>(*scene,
                                                  "{68500000-0000-0000-0000-000000000002}"));
    EXPECT_NE(nullptr, requireElement<DirectionalLight>(*scene,
                                                        "{68500000-0000-0000-0000-000000000003}"));
    EXPECT_NE(nullptr, requireElement<Rectangle>(*scene,
                                                 "{68500000-0000-0000-0000-000000000030}"));
    EXPECT_NE(nullptr, requireElement<Rectangle>(*scene,
                                                 "{68500000-0000-0000-0000-000000000031}"));
    EXPECT_NE(nullptr, requireElement<Sphere>(*scene,
                                              "{68500000-0000-0000-0000-000000000040}"));
    EXPECT_NE(nullptr, requireElement<Torus>(*scene,
                                             "{68500000-0000-0000-0000-000000000041}"));
    EXPECT_NE(nullptr, requireElement<OpenCylinder>(*scene,
                                                    "{68500000-0000-0000-0000-000000000042}"));
    EXPECT_NE(nullptr, requireElement<Disk>(*scene,
                                            "{68500000-0000-0000-0000-000000000043}"));

    const auto runtimeScene = scene->toRaytracerScene();
    const render::CompiledIntersectionScene compiled =
      render::IntersectionSceneCompiler().compile(*runtimeScene);

    EXPECT_TRUE(compiled.fullySupported());
    EXPECT_TRUE(compiled.unsupportedPrimitives().empty());
    EXPECT_EQ(1u, compiled.spheres().size());
    EXPECT_EQ(2u, compiled.rectangles().size());
    EXPECT_EQ(1u, compiled.disks().size());
    EXPECT_EQ(1u, compiled.openCylinders().size());
    EXPECT_EQ(1u, compiled.tori().size());
  }

  TEST(TracingExecutionInspectionScene, EmitsWavefrontBackendTraceMetadata) {
    auto scene = loadScene();
    ASSERT_NE(nullptr, scene);

    auto* camera = requireElement<Camera>(*scene, cameraId);
    ASSERT_NE(nullptr, camera);

    engine::graph::RenderIntent intent = scene->renderIntentWithActiveCameraDefault();
    ASSERT_TRUE(intent.engineOptions.raytracer().intersectionBackend().has_value());
    EXPECT_EQ(engine::graph::RenderExecutorPreference::PathTracer, intent.defaultExecutor);
    EXPECT_STREQ("gpu", intent.engineOptions.raytracer().intersectionBackend()->id());

    engine::graph::GraphRenderEngine engine(camera->toRaytracer(), scene->toRaytracerScene());
    engine.setExecutionTraceEnabled(true);
    engine.setSceneAnalysis(scene->renderGraphAnalysis());
    engine.setPlan(engine::graph::RenderGraphCompiler().compile({8, 8, 1}, intent));

    Buffer<Colord> buffer(8, 8);
    engine.render(buffer);

    const auto trace = engine.lastExecutionTrace();
    ASSERT_NE(nullptr, trace);
    const engine::graph::RenderPassTrace* wavefront = trace->findPass("wavefront_beauty");
    ASSERT_NE(nullptr, wavefront);

    const QJsonObject batching = wavefront->metadata().value("batching").toObject();
    EXPECT_EQ(QStringLiteral("pathtracer"), batching.value("integrator").toString());
    EXPECT_EQ(QStringLiteral("gpu"), batching.value("intersectionBackendRequest").toString());
    EXPECT_FALSE(batching.value("intersectionBackend").toString().isEmpty());
    EXPECT_FALSE(batching.value("intersectionBackendExecutionPath").toString().isEmpty());
    EXPECT_GT(batching.value("intersectionBackendExpectedRays").toDouble(), 0.0);
    EXPECT_GT(batching.value("intersectionScenePrimitives").toDouble(), 0.0);
    EXPECT_EQ(1.0, batching.value("intersectionSceneSpheres").toDouble());
    EXPECT_EQ(2.0, batching.value("intersectionSceneRectangles").toDouble());
    EXPECT_EQ(1.0, batching.value("intersectionSceneDisks").toDouble());
    EXPECT_EQ(1.0, batching.value("intersectionSceneOpenCylinders").toDouble());
    EXPECT_EQ(1.0, batching.value("intersectionSceneTori").toDouble());
  }
}
