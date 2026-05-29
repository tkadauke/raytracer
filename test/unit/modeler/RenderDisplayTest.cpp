#include <gtest/gtest.h>

#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/RenderGraphTypes.h"
#include "engine/raster/RasterBackend.h"
#include "src/modeler/Display.h"
#include "render/RenderEngine.h"
#include "render/cameras/Camera.h"
#include "test/helpers/GuiTestHelper.h"
#include "test/helpers/VectorTestHelper.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/Scene.h"
#include "world/objects/StepVisibilityEvaluator.h"

#include <memory>

namespace RenderDisplayTest {
  class RenderDisplayTest : public ::testing::GuiTest {};

  Scene* sceneWithCamera(PinholeCamera** cameraOut) {
    auto* scene = new Scene;
    auto* camera = new PinholeCamera;
    camera->setPosition(Vector3d(0, 0, -6));
    camera->setTarget(Vector3d::null);
    scene->addChild(camera);
    *cameraOut = camera;
    return scene;
  }

  TEST_F(RenderDisplayTest, ShouldPreservePreviewCameraWhenSceneIsRefreshed) {
    PinholeCamera* sceneCamera = nullptr;
    std::unique_ptr<Scene> scene(sceneWithCamera(&sceneCamera));

    RenderDisplay display(nullptr);
    display.setRenderGraphPreviewEnabled(false);
    display.setScene(scene.get());

    const Vector3d previewPosition(3, 4, -8);
    const Vector3d previewTarget(1, 2, 3);
    display.renderEngine()->camera()->setPosition(previewPosition);
    display.renderEngine()->camera()->setTarget(previewTarget);

    sceneCamera->setPosition(Vector3d(0, 0, -2));
    sceneCamera->setTarget(Vector3d(0, 1, 0));
    display.setScene(scene.get(), StepPlaybackStyle(),
                     RenderDisplay::CameraPolicy::PreserveCurrent);

    ASSERT_VECTOR_NEAR(previewPosition, display.renderEngine()->camera()->position(), 1e-9);
    ASSERT_VECTOR_NEAR(previewTarget, display.renderEngine()->camera()->target(), 1e-9);
  }

  TEST_F(RenderDisplayTest, ShouldResetPreviewCameraWhenSceneIsOpened) {
    PinholeCamera* sceneCamera = nullptr;
    std::unique_ptr<Scene> scene(sceneWithCamera(&sceneCamera));

    RenderDisplay display(nullptr);
    display.setRenderGraphPreviewEnabled(false);
    display.setScene(scene.get());
    display.renderEngine()->camera()->setPosition(Vector3d(3, 4, -8));
    display.renderEngine()->camera()->setTarget(Vector3d(1, 2, 3));

    sceneCamera->setPosition(Vector3d(0, 0, -2));
    sceneCamera->setTarget(Vector3d(0, 1, 0));
    display.setScene(scene.get(), StepPlaybackStyle(),
                     RenderDisplay::CameraPolicy::ResetToSceneCamera);

    ASSERT_VECTOR_NEAR(sceneCamera->position(), display.renderEngine()->camera()->position(), 1e-9);
    ASSERT_VECTOR_NEAR(sceneCamera->target(), display.renderEngine()->camera()->target(), 1e-9);
  }

  TEST_F(RenderDisplayTest, ShouldStoreRasterizerPreviewBackend) {
    RenderDisplay display(nullptr);

    EXPECT_TRUE(display.rasterizerPreviewBackend().isCPU());

    display.setRenderGraphPreviewEnabled(false);
    display.setRasterizerPreviewBackend(engine::raster::RasterBackend::openGL());

    EXPECT_TRUE(display.rasterizerPreviewBackend().isOpenGL());
  }

  TEST_F(RenderDisplayTest, ShouldBindSceneCamerasForPreviewGraphPasses) {
    auto scene = std::make_unique<Scene>();
    auto* activeCamera = new PinholeCamera;
    activeCamera->setId("active-camera");
    activeCamera->setPosition(Vector3d(0, 0, -6));
    activeCamera->setTarget(Vector3d::null);
    scene->addChild(activeCamera);

    auto* passCamera = new PinholeCamera;
    passCamera->setId("pass-camera");
    passCamera->setPosition(Vector3d(4, 3, -9));
    passCamera->setTarget(Vector3d(1, 2, 0));
    scene->addChild(passCamera);

    RenderDisplay display(nullptr);
    display.setRenderGraphPreviewEnabled(false);
    display.setScene(scene.get());

    auto graph =
      std::dynamic_pointer_cast<engine::graph::GraphRenderEngine>(display.renderEngine());
    ASSERT_NE(nullptr, graph);

    engine::graph::RenderPassNode pass;
    pass.sceneView.camera = engine::graph::RenderCameraRef{"pass-camera", std::nullopt};

    auto selectedCamera = graph->cameraForPass(pass);
    ASSERT_NE(nullptr, selectedCamera);
    ASSERT_VECTOR_NEAR(passCamera->position(), selectedCamera->position(), 1e-9);
    ASSERT_VECTOR_NEAR(passCamera->target(), selectedCamera->target(), 1e-9);
  }

  // Regression guard: `RenderDisplay::setScene` used to bind a snapshot of the
  // active scene camera to the graph engine and then create a *second*
  // independent snapshot for the engine's runtime camera. Interactive mouse
  // drags mutate the runtime camera, but graph passes that name the active
  // camera by id were resolving to the stale binding — so all engines kept
  // rendering with the pre-drag pose.
  TEST_F(RenderDisplayTest, ActiveSceneCameraBindingSharesEngineRuntimeCamera) {
    auto scene = std::make_unique<Scene>();
    auto* activeCamera = new PinholeCamera;
    activeCamera->setId("active-camera");
    activeCamera->setPosition(Vector3d(0, 0, -6));
    activeCamera->setTarget(Vector3d::null);
    scene->addChild(activeCamera);

    RenderDisplay display(nullptr);
    display.setRenderGraphPreviewEnabled(false);
    display.setScene(scene.get());

    auto graph =
      std::dynamic_pointer_cast<engine::graph::GraphRenderEngine>(display.renderEngine());
    ASSERT_NE(nullptr, graph);

    engine::graph::RenderPassNode pass;
    pass.sceneView.camera = engine::graph::RenderCameraRef{"active-camera", std::nullopt};

    EXPECT_EQ(display.renderEngine()->camera().get(), graph->cameraForPass(pass).get());
  }

  TEST_F(RenderDisplayTest, MutatingEngineCameraIsVisibleThroughCameraForPass) {
    auto scene = std::make_unique<Scene>();
    auto* activeCamera = new PinholeCamera;
    activeCamera->setId("active-camera");
    activeCamera->setPosition(Vector3d(0, 0, -6));
    activeCamera->setTarget(Vector3d::null);
    scene->addChild(activeCamera);

    RenderDisplay display(nullptr);
    display.setRenderGraphPreviewEnabled(false);
    display.setScene(scene.get());

    auto graph =
      std::dynamic_pointer_cast<engine::graph::GraphRenderEngine>(display.renderEngine());
    ASSERT_NE(nullptr, graph);

    const Vector3d draggedPosition(7, 8, 9);
    const Vector3d draggedTarget(1, -1, 2);
    display.renderEngine()->camera()->setPosition(draggedPosition);
    display.renderEngine()->camera()->setTarget(draggedTarget);

    engine::graph::RenderPassNode pass;
    pass.sceneView.camera = engine::graph::RenderCameraRef{"active-camera", std::nullopt};

    auto resolved = graph->cameraForPass(pass);
    ASSERT_NE(nullptr, resolved);
    ASSERT_VECTOR_NEAR(draggedPosition, resolved->position(), 1e-9);
    ASSERT_VECTOR_NEAR(draggedTarget, resolved->target(), 1e-9);
  }

} // namespace RenderDisplayTest
