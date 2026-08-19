#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "mcp/SceneEditor.h"

#include "widgets/world/SceneModel.h"

#include "world/objects/Box.h"
#include "world/objects/Difference.h"
#include "world/objects/FishEyeCamera.h"
#include "world/objects/Intersection.h"
#include "world/objects/MatteMaterial.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"
#include "world/objects/Surface.h"
#include "world/objects/Union.h"

#include "test/helpers/GuiTestHelper.h"

namespace SceneEditorTest {

  namespace {
    QJsonArray vec(double x, double y, double z) {
      return QJsonArray{x, y, z};
    }

    // QJsonValue's default constructor builds a *Null* value, not an
    // *Undefined* one — only QJsonObject::value() on a missing key returns
    // Undefined. SceneEditor treats "not provided" as isUndefined(), so
    // tests exercising "this argument was omitted" need this sentinel
    // instead of a bare QJsonValue().
    QJsonValue omitted() {
      return QJsonValue(QJsonValue::Undefined);
    }
  }

  class SceneEditorTest : public ::testing::GuiTest {
  protected:
    void SetUp() override {
      ::testing::GuiTest::SetUp();
      scene = new Scene;
      model = std::make_unique<SceneModel>(scene);
      selectionModel = std::make_unique<QItemSelectionModel>(model.get());
      editor = std::make_unique<mcp::SceneEditor>([this]() -> Scene* { return scene; },
                                                  model.get(), selectionModel.get());

      QObject::connect(editor.get(), &mcp::SceneEditor::elementChanged,
                       [this](Element* element) { elementChangedCalls.push_back(element); });
    }

    Scene* scene = nullptr;
    std::unique_ptr<SceneModel> model;
    std::unique_ptr<QItemSelectionModel> selectionModel;
    std::unique_ptr<mcp::SceneEditor> editor;
    std::vector<Element*> elementChangedCalls;
  };

  TEST_F(SceneEditorTest, AddPrimitiveCreatesElementAtPosition) {
    const auto result = editor->addPrimitive(QStringLiteral("Box"), vec(1, 2, 3), QJsonObject());

    EXPECT_TRUE(result.ok);
    ASSERT_FALSE(result.id.isEmpty());
    EXPECT_EQ(1u, elementChangedCalls.size());

    auto* box = qobject_cast<Box*>(scene->findById(result.id));
    ASSERT_NE(nullptr, box);
    EXPECT_EQ(Vector3d(1, 2, 3), box->position());
    EXPECT_EQ(box, elementChangedCalls.back());
  }

  TEST_F(SceneEditorTest, AddPrimitiveAppliesExtraParams) {
    QJsonObject params;
    params[QStringLiteral("size")] = vec(2, 3, 4);
    const auto result = editor->addPrimitive(QStringLiteral("Box"), omitted(), params);

    ASSERT_TRUE(result.ok);
    auto* box = qobject_cast<Box*>(scene->findById(result.id));
    ASSERT_NE(nullptr, box);
    EXPECT_EQ(Vector3d(2, 3, 4), box->size());
  }

  TEST_F(SceneEditorTest, AddPrimitiveUsesTheCurrentSceneAfterItIsReplaced) {
    // Mirrors MainWindow::newFile(): the old Scene is deleted and p->scene
    // repointed at a fresh one, exactly like File > New. SceneEditor must
    // re-invoke its SceneProvider rather than caching the pointer it saw at
    // construction time, or this would mutate a dangling Scene.
    auto* replacementScene = new Scene;
    delete scene;
    scene = replacementScene;
    model->setElement(scene);

    const auto result = editor->addPrimitive(QStringLiteral("Box"), omitted(), QJsonObject());

    EXPECT_TRUE(result.ok);
    EXPECT_NE(nullptr, scene->findById(result.id));
  }

  TEST_F(SceneEditorTest, AddPrimitiveRejectsUnsupportedType) {
    const auto result =
      editor->addPrimitive(QStringLiteral("Scene"), omitted(), QJsonObject());

    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(elementChangedCalls.empty());
  }

  TEST_F(SceneEditorTest, TransformSetsPositionRotationAndScale) {
    auto* box = new Box;
    scene->addChild(box);

    const auto result =
      editor->transform(box->id(), vec(1, 0, 0), vec(0, 0, 1.5), vec(2, 2, 2));

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(Vector3d(1, 0, 0), box->position());
    EXPECT_EQ(Vector3d(0, 0, 1.5), box->rotation());
    EXPECT_EQ(Vector3d(2, 2, 2), box->scale());
    EXPECT_EQ(1u, elementChangedCalls.size());
  }

  TEST_F(SceneEditorTest, TransformFailsForUnknownId) {
    const auto result = editor->transform(QStringLiteral("nope"), vec(1, 0, 0), omitted(),
                                          omitted());
    EXPECT_FALSE(result.ok);
  }

  TEST_F(SceneEditorTest, TransformFailsForNonTransformableElement) {
    auto* material = new MatteMaterial;
    scene->addChild(material);

    const auto result = editor->transform(material->id(), vec(1, 0, 0), omitted(), omitted());
    EXPECT_FALSE(result.ok);
  }

  TEST_F(SceneEditorTest, ApplyMaterialAttachesExistingMaterialById) {
    auto* box = new Box;
    auto* material = new MatteMaterial;
    scene->addChild(box);
    scene->addChild(material);

    const auto result = editor->applyMaterial(box->id(), material->id());

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(material, box->material());
    EXPECT_EQ(1u, elementChangedCalls.size());
  }

  TEST_F(SceneEditorTest, ApplyMaterialCreatesInlineMaterial) {
    auto* box = new Box;
    scene->addChild(box);

    QJsonObject inlineMaterial;
    inlineMaterial[QStringLiteral("type")] = QStringLiteral("MatteMaterial");
    QJsonObject params;
    params[QStringLiteral("ambientCoefficient")] = 0.5;
    inlineMaterial[QStringLiteral("params")] = params;

    const auto result = editor->applyMaterial(box->id(), inlineMaterial);

    ASSERT_TRUE(result.ok);
    auto* material = qobject_cast<MatteMaterial*>(scene->findById(result.id));
    ASSERT_NE(nullptr, material);
    EXPECT_EQ(box->material(), material);
    EXPECT_DOUBLE_EQ(0.5, material->ambientCoefficient());
  }

  TEST_F(SceneEditorTest, ApplyMaterialRejectsUnsupportedInlineType) {
    auto* box = new Box;
    scene->addChild(box);

    QJsonObject inlineMaterial;
    inlineMaterial[QStringLiteral("type")] = QStringLiteral("ReflectiveMaterial");

    const auto result = editor->applyMaterial(box->id(), inlineMaterial);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(nullptr, box->material());
  }

  TEST_F(SceneEditorTest, ApplyMaterialFailsForNonSurfaceTarget) {
    auto* camera = new PinholeCamera;
    scene->addChild(camera);

    const auto result = editor->applyMaterial(camera->id(), QStringLiteral("whatever"));
    EXPECT_FALSE(result.ok);
  }

  TEST_F(SceneEditorTest, SelectDrivesSelectionModelAndFiresCurrentChanged) {
    auto* box = new Box;
    scene->addChild(box);

    int currentChangedCount = 0;
    QObject::connect(selectionModel.get(), &QItemSelectionModel::currentChanged,
                     [&currentChangedCount](const QModelIndex&, const QModelIndex&) {
                       ++currentChangedCount;
                     });

    const auto result = editor->select(box->id());

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(1, currentChangedCount);
    ASSERT_TRUE(selectionModel->currentIndex().isValid());
    EXPECT_EQ(box, static_cast<Element*>(selectionModel->currentIndex().internalPointer()));
  }

  TEST_F(SceneEditorTest, SelectWithEmptyIdClearsSelection) {
    auto* box = new Box;
    scene->addChild(box);
    ASSERT_TRUE(editor->select(box->id()).ok);

    const auto result = editor->select(QString());

    EXPECT_TRUE(result.ok);
    EXPECT_FALSE(selectionModel->currentIndex().isValid());
  }

  TEST_F(SceneEditorTest, SelectFailsForUnknownId) {
    const auto result = editor->select(QStringLiteral("nope"));
    EXPECT_FALSE(result.ok);
  }

  TEST_F(SceneEditorTest, DeleteRemovesElementFromSceneAndFiresRowsRemoved) {
    auto* box = new Box;
    scene->addChild(box);
    const QString id = box->id();

    int rowsRemovedCount = 0;
    QObject::connect(model.get(), &SceneModel::rowsRemoved,
                     [&rowsRemovedCount](const QModelIndex&, int, int) { ++rowsRemovedCount; });

    const auto result = editor->deleteElement(id);

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(1, rowsRemovedCount);
    EXPECT_EQ(nullptr, scene->findById(id));
    ASSERT_EQ(1u, elementChangedCalls.size());
    EXPECT_EQ(nullptr, elementChangedCalls.back());
  }

  TEST_F(SceneEditorTest, DeleteRejectsSceneRoot) {
    const auto result = editor->deleteElement(scene->id());
    EXPECT_FALSE(result.ok);
  }

  TEST_F(SceneEditorTest, DeleteFailsForUnknownId) {
    const auto result = editor->deleteElement(QStringLiteral("nope"));
    EXPECT_FALSE(result.ok);
  }

  TEST_F(SceneEditorTest, CsgUnionReparentsBothOperandsUnderNewUnion) {
    auto* box = new Box;
    auto* sphere = new Sphere;
    scene->addChild(box);
    scene->addChild(sphere);

    const auto result = editor->csgUnion(box->id(), sphere->id());

    ASSERT_TRUE(result.ok);
    auto* unionElement = qobject_cast<Union*>(scene->findById(result.id));
    ASSERT_NE(nullptr, unionElement);
    EXPECT_EQ(unionElement, box->parent());
    EXPECT_EQ(unionElement, sphere->parent());
    EXPECT_EQ(2, unionElement->childElements().size());
  }

  TEST_F(SceneEditorTest, CsgIntersectReparentsBothOperands) {
    auto* box = new Box;
    auto* sphere = new Sphere;
    scene->addChild(box);
    scene->addChild(sphere);

    const auto result = editor->csgIntersect(box->id(), sphere->id());

    ASSERT_TRUE(result.ok);
    auto* intersection = qobject_cast<Intersection*>(scene->findById(result.id));
    ASSERT_NE(nullptr, intersection);
    EXPECT_EQ(intersection, box->parent());
    EXPECT_EQ(intersection, sphere->parent());
  }

  TEST_F(SceneEditorTest, CsgDifferenceReparentsBothOperands) {
    auto* box = new Box;
    auto* sphere = new Sphere;
    scene->addChild(box);
    scene->addChild(sphere);

    const auto result = editor->csgDifference(box->id(), sphere->id());

    ASSERT_TRUE(result.ok);
    auto* difference = qobject_cast<Difference*>(scene->findById(result.id));
    ASSERT_NE(nullptr, difference);
    EXPECT_EQ(difference, box->parent());
    EXPECT_EQ(difference, sphere->parent());
  }

  TEST_F(SceneEditorTest, CsgUnionRejectsSameOperandTwice) {
    auto* box = new Box;
    scene->addChild(box);

    const auto result = editor->csgUnion(box->id(), box->id());
    EXPECT_FALSE(result.ok);
  }

  TEST_F(SceneEditorTest, CsgUnionRejectsNonSurfaceOperand) {
    auto* box = new Box;
    auto* camera = new PinholeCamera;
    scene->addChild(box);
    scene->addChild(camera);

    const auto result = editor->csgUnion(box->id(), camera->id());
    EXPECT_FALSE(result.ok);
  }

  TEST_F(SceneEditorTest, SetCameraSetsPositionAndTarget) {
    auto* camera = new PinholeCamera;
    scene->addChild(camera);

    const auto result = editor->setCamera(vec(0, 1, 5), vec(0, 0, 0), omitted());

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(Vector3d(0, 1, 5), camera->position());
    EXPECT_EQ(Vector3d(0, 0, 0), camera->target());
    EXPECT_EQ(1u, elementChangedCalls.size());
  }

  TEST_F(SceneEditorTest, SetCameraAppliesFovWhenCameraSupportsFieldOfView) {
    auto* camera = new FishEyeCamera;
    scene->addChild(camera);

    const auto result = editor->setCamera(omitted(), omitted(), 90.0);

    ASSERT_TRUE(result.ok);
    EXPECT_NEAR(90.0, camera->fieldOfView().degrees(), 1e-6);
  }

  TEST_F(SceneEditorTest, SetCameraNotesWhenFovIsUnsupportedButOtherFieldsApply) {
    auto* camera = new PinholeCamera;
    scene->addChild(camera);

    const auto result = editor->setCamera(vec(0, 1, 5), omitted(), 90.0);

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(Vector3d(0, 1, 5), camera->position());
    EXPECT_FALSE(result.message.isEmpty());
  }

  TEST_F(SceneEditorTest, SetCameraFailsWhenOnlyUnsupportedFovIsGiven) {
    auto* camera = new PinholeCamera;
    scene->addChild(camera);

    const auto result = editor->setCamera(omitted(), omitted(), 90.0);

    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.message.isEmpty());
  }

  TEST_F(SceneEditorTest, SetCameraFailsWhenSceneHasNoCamera) {
    const auto result = editor->setCamera(vec(0, 0, 0), omitted(), omitted());
    EXPECT_FALSE(result.ok);
  }

}
