#include <gtest/gtest.h>

#include "widgets/world/SceneModel.h"
#include "world/objects/Scene.h"
#include "world/objects/PinholeCamera.h"

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
    EXPECT_EQ(1, model.rowCount(root));
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
    auto child = model.index(0, 0, root);
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
}
