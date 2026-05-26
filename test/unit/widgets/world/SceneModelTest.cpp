#include <gtest/gtest.h>

#include "widgets/world/SceneModel.h"
#include "world/objects/Scene.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/Group.h"
#include "world/objects/MatteMaterial.h"
#include "world/objects/RenderIntentElement.h"
#include "world/objects/Sphere.h"
#include "core/math/Vector.h"

#include "test/helpers/GuiTestHelper.h"

namespace SceneModelTest {
  class SceneModelTest : public ::testing::GuiTest {};

  TEST_F(SceneModelTest, ShouldInitializeWithScene) {
    auto* scene = new Scene;
    SceneModel model(scene);
    // The model wraps the scene as the only child of an internal hidden
    // root, so the top-level row count from QModelIndex() is 1.
    EXPECT_EQ(1, model.rowCount(QModelIndex()));
  }

  TEST_F(SceneModelTest, ShouldHaveSingleColumn) {
    auto* scene = new Scene;
    SceneModel model(scene);
    EXPECT_EQ(1, model.columnCount(QModelIndex()));
  }

  TEST_F(SceneModelTest, ShouldReturnSceneAsTopLevelIndex) {
    auto* scene = new Scene;
    SceneModel model(scene);
    auto root = model.index(0, 0, QModelIndex());
    ASSERT_TRUE(root.isValid());
    EXPECT_EQ(scene, static_cast<Element*>(root.internalPointer()));
  }

  TEST_F(SceneModelTest, ShouldReturnDisplayNameAsData) {
    auto* scene = new Scene;
    scene->setName("My Scene");
    SceneModel model(scene);
    auto root = model.index(0, 0, QModelIndex());
    EXPECT_EQ(QString("My Scene"), model.data(root, Qt::DisplayRole).toString());
  }

  TEST_F(SceneModelTest, ShouldReturnInvalidVariantForNonDisplayRole) {
    auto* scene = new Scene;
    SceneModel model(scene);
    auto root = model.index(0, 0, QModelIndex());
    EXPECT_FALSE(model.data(root, Qt::ToolTipRole).isValid());
  }

  TEST_F(SceneModelTest, ShouldExposeChildrenUnderScene) {
    auto* scene = new Scene;
    auto* camera = new PinholeCamera;
    scene->addChild(camera);

    SceneModel model(scene);
    auto root = model.index(0, 0, QModelIndex());
    EXPECT_EQ(2, model.rowCount(root));
  }

  TEST_F(SceneModelTest, ShouldExposeGeneratedRenderIntentUnderScene) {
    auto* scene = new Scene;

    SceneModel model(scene);
    auto root = model.index(0, 0, QModelIndex());
    auto intent = model.index(0, 0, root);

    ASSERT_TRUE(intent.isValid());
    EXPECT_NE(nullptr,
              qobject_cast<RenderIntentElement*>(static_cast<Element*>(intent.internalPointer())));
    EXPECT_EQ(QString("Render Intent"), model.data(intent, Qt::DisplayRole).toString());
  }

  TEST_F(SceneModelTest, ShouldReturnInvalidParentForTopLevelIndex) {
    auto* scene = new Scene;
    SceneModel model(scene);
    auto root = model.index(0, 0, QModelIndex());
    // The scene's parent is the hidden m_rootItem, which the model maps
    // back to QModelIndex() (sentinel for "no parent in the model's
    // view") — so view code can identify the top-level row.
    EXPECT_FALSE(model.parent(root).isValid());
  }

  TEST_F(SceneModelTest, ShouldReturnSceneAsParentOfChildIndex) {
    auto* scene = new Scene;
    auto* camera = new PinholeCamera;
    scene->addChild(camera);

    SceneModel model(scene);
    auto root = model.index(0, 0, QModelIndex());
    auto child = model.index(1, 0, root);
    auto childParent = model.parent(child);
    ASSERT_TRUE(childParent.isValid());
    EXPECT_EQ(scene, static_cast<Element*>(childParent.internalPointer()));
  }

  TEST_F(SceneModelTest, ShouldReturnInvalidIndexForOutOfRangeRow) {
    auto* scene = new Scene;
    SceneModel model(scene);
    EXPECT_FALSE(model.index(5, 0, QModelIndex()).isValid());
  }

  TEST_F(SceneModelTest, ShouldReturnInvalidVariantForInvalidIndex) {
    auto* scene = new Scene;
    SceneModel model(scene);
    EXPECT_FALSE(model.data(QModelIndex()).isValid());
  }

  TEST_F(SceneModelTest, ShouldDeclareMimeTypesForDragDrop) {
    auto* scene = new Scene;
    SceneModel model(scene);
    // Drag-and-drop reparenting in the scene tree relies on a non-empty
    // mime-type list — Qt's view code refuses to start a drag otherwise.
    EXPECT_FALSE(model.mimeTypes().isEmpty());
  }

  TEST_F(SceneModelTest, ShouldSupportMoveDropAction) {
    auto* scene = new Scene;
    SceneModel model(scene);
    EXPECT_TRUE(model.supportedDropActions() & Qt::MoveAction);
  }

  TEST_F(SceneModelTest, ShouldReplaceSceneOnSetElement) {
    auto* scene1 = new Scene;
    SceneModel model(scene1);
    auto* scene2 = new Scene;
    model.setElement(scene2);
    auto root = model.index(0, 0, QModelIndex());
    EXPECT_EQ(scene2, static_cast<Element*>(root.internalPointer()));
  }

  TEST_F(SceneModelTest, ShouldMoveSurfaceIntoGroup) {
    auto* scene = new Scene;
    auto* group = new Group;
    auto* sphere = new Sphere;
    scene->addChild(group);
    scene->addChild(sphere);

    SceneModel model(scene);
    auto root = model.index(0, 0, QModelIndex());
    auto groupIndex = model.index(1, 0, root);

    EXPECT_TRUE(model.moveRow(root, 2, groupIndex, 0));
    EXPECT_EQ(group, sphere->parent());
    EXPECT_EQ(sphere, group->childElements().first());
  }

  TEST_F(SceneModelTest, ShouldRejectMoveIntoGroupWhenChildTypeIsNotAllowed) {
    auto* scene = new Scene;
    auto* group = new Group;
    auto* material = new MatteMaterial;
    scene->addChild(group);
    scene->addChild(material);

    SceneModel model(scene);
    auto root = model.index(0, 0, QModelIndex());
    auto groupIndex = model.index(1, 0, root);

    EXPECT_FALSE(model.moveRow(root, 2, groupIndex, 0));
    EXPECT_EQ(scene, material->parent());
    EXPECT_TRUE(group->childElements().isEmpty());
  }

  TEST_F(SceneModelTest, ShouldMoveChildOutOfGroupToScene) {
    auto* scene = new Scene;
    auto* group = new Group;
    auto* sphere = new Sphere;
    scene->addChild(group);
    group->addChild(sphere);

    SceneModel model(scene);
    auto root = model.index(0, 0, QModelIndex());
    auto groupIndex = model.index(1, 0, root);

    EXPECT_TRUE(model.moveRow(groupIndex, 0, root, 2));
    EXPECT_EQ(scene, sphere->parent());
    EXPECT_EQ(sphere, scene->childElements().last());
  }

  TEST_F(SceneModelTest, ShouldRejectMovingGroupIntoItsDescendant) {
    auto* scene = new Scene;
    auto* parentGroup = new Group;
    auto* childGroup = new Group;
    scene->addChild(parentGroup);
    parentGroup->addChild(childGroup);

    SceneModel model(scene);
    auto root = model.index(0, 0, QModelIndex());
    auto parentGroupIndex = model.index(1, 0, root);
    auto childGroupIndex = model.index(0, 0, parentGroupIndex);

    EXPECT_FALSE(model.moveRow(root, 1, childGroupIndex, 0));
    EXPECT_EQ(scene, parentGroup->parent());
    EXPECT_EQ(parentGroup, childGroup->parent());
  }

  TEST_F(SceneModelTest, ShouldPreserveGlobalTransformWhenReparentingIntoGroup) {
    auto* scene = new Scene;
    auto* group = new Group;
    auto* sphere = new Sphere;
    group->setPosition(Vector3d(10, 0, 0));
    sphere->setPosition(Vector3d(2, 0, 0));
    scene->addChild(group);
    scene->addChild(sphere);

    const auto before = sphere->globalTransform();

    SceneModel model(scene);
    auto root = model.index(0, 0, QModelIndex());
    auto groupIndex = model.index(1, 0, root);

    ASSERT_TRUE(model.moveRow(root, 2, groupIndex, 0));
    EXPECT_EQ(before, sphere->globalTransform());
    EXPECT_EQ(Vector3d(-8, 0, 0), sphere->position());
  }
}
