#include <gtest/gtest.h>

#include "world/objects/Transformable.h"
#include "world/objects/Element.h"
#include "core/math/Matrix.h"
#include "core/math/Vector.h"

#include "test/helpers/MatrixTestHelper.h"
#include "test/helpers/VectorTestHelper.h"

namespace TransformableTest {
  TEST(Transformable, ShouldDefaultPositionToOrigin) {
    Transformable t;
    EXPECT_EQ(Vector3d(0, 0, 0), t.position());
  }

  TEST(Transformable, ShouldDefaultRotationToZero) {
    Transformable t;
    EXPECT_EQ(Vector3d(0, 0, 0), t.rotation());
  }

  TEST(Transformable, ShouldDefaultScaleToOne) {
    Transformable t;
    EXPECT_EQ(Vector3d(1, 1, 1), t.scale());
  }

  TEST(Transformable, ShouldSetAndGetPosition) {
    Transformable t;
    t.setPosition(Vector3d(1, 2, 3));
    EXPECT_EQ(Vector3d(1, 2, 3), t.position());
  }

  TEST(Transformable, ShouldSetAndGetRotation) {
    Transformable t;
    t.setRotation(Vector3d(0.1, 0.2, 0.3));
    EXPECT_EQ(Vector3d(0.1, 0.2, 0.3), t.rotation());
  }

  TEST(Transformable, ShouldSetAndGetScale) {
    Transformable t;
    t.setScale(Vector3d(2, 3, 4));
    EXPECT_EQ(Vector3d(2, 3, 4), t.scale());
  }

  TEST(Transformable, ShouldClampNearZeroScaleToMinimum) {
    Transformable t;
    t.setScale(Vector3d(0, 0, 0));
    // setScale enforces a 1e-6 floor on each axis to keep the scale
    // matrix invertible — a zero scale would make globalTransform()
    // singular and break leaveParent/joinParent's setMatrix decode.
    EXPECT_DOUBLE_EQ(1e-6, t.scale().x());
    EXPECT_DOUBLE_EQ(1e-6, t.scale().y());
    EXPECT_DOUBLE_EQ(1e-6, t.scale().z());
  }

  TEST(Transformable, ShouldTakeAbsoluteValueOfNegativeScale) {
    Transformable t;
    t.setScale(Vector3d(-2, -3, -4));
    EXPECT_EQ(Vector3d(2, 3, 4), t.scale());
  }

  TEST(Transformable, ShouldAllowTransformableChild) {
    Transformable parent;
    Transformable child;
    EXPECT_TRUE(parent.canHaveChild(&child));
  }

  TEST(Transformable, ShouldRejectNonTransformableChild) {
    Transformable parent;
    Element child;
    EXPECT_FALSE(parent.canHaveChild(&child));
  }

  TEST(Transformable, ShouldReturnIdentityLocalTransformByDefault) {
    Transformable t;
    ASSERT_MATRIX_NEAR(Matrix4d(), t.localTransform(), 1e-9);
  }

  TEST(Transformable, ShouldEncodePositionInLocalTransform) {
    Transformable t;
    t.setPosition(Vector3d(5, 7, -3));
    auto m = t.localTransform();
    ASSERT_VECTOR_NEAR(Vector3d(5, 7, -3), m.translationVector(), 1e-9);
  }

  TEST(Transformable, ShouldEncodeScaleInLocalTransform) {
    Transformable t;
    t.setScale(Vector3d(2, 3, 4));
    auto m = t.localTransform();
    ASSERT_VECTOR_NEAR(Vector3d(2, 3, 4), Matrix3d(m).scaleVector(), 1e-9);
  }

  TEST(Transformable, ShouldComposeTranslationAndScaleInLocalTransform) {
    Transformable t;
    t.setPosition(Vector3d(1, 2, 3));
    t.setScale(Vector3d(2, 2, 2));
    auto m = t.localTransform();
    // Local transform = translate * rotate * scale; the unit point
    // (1,0,0) — promoted to homogeneous (1,0,0,1) — maps to
    // (translate + scale * (1,0,0)) = (3,2,3).
    Vector3d image(m * Vector4d(1, 0, 0, 1));
    ASSERT_VECTOR_NEAR(Vector3d(3, 2, 3), image, 1e-9);
  }

  TEST(Transformable, ShouldEqualLocalGlobalTransformWhenNoTransformableParent) {
    Transformable t;
    t.setPosition(Vector3d(1, 2, 3));
    t.setScale(Vector3d(2, 3, 4));
    ASSERT_MATRIX_NEAR(t.localTransform(), t.globalTransform(), 1e-9);
  }

  TEST(Transformable, ShouldComposeGlobalTransformWithTransformableParent) {
    auto* parent = new Transformable;
    parent->setPosition(Vector3d(10, 0, 0));

    auto* child = new Transformable;
    parent->addChild(child);
    // Position the child *after* addChild so the local position is
    // interpreted in the parent's frame (joinParent rebases an existing
    // local transform to preserve world position — see the next test).
    child->setPosition(Vector3d(0, 5, 0));

    // Global = parent.local * child.local; the unit-x point at the child
    // origin maps to parent + child + (1,0,0) = (11, 5, 0).
    Vector3d image(child->globalTransform() * Vector4d(1, 0, 0, 1));
    ASSERT_VECTOR_NEAR(Vector3d(11, 5, 0), image, 1e-9);
  }

  TEST(Transformable, ShouldPreserveChildWorldPositionOnAddChild) {
    // joinParent's contract: when a Transformable becomes a child of
    // another Transformable, its *world* position is preserved. The local
    // transform is rebased through the new parent's global inverse so
    // child.global stays put across the reparent.
    auto* parent = new Transformable;
    parent->setPosition(Vector3d(10, 0, 0));

    auto* child = new Transformable;
    child->setPosition(Vector3d(0, 5, 0));
    Vector3d worldBefore(child->globalTransform() * Vector4d(0, 0, 0, 1));

    parent->addChild(child);
    Vector3d worldAfter(child->globalTransform() * Vector4d(0, 0, 0, 1));

    ASSERT_VECTOR_NEAR(worldBefore, worldAfter, 1e-9);
  }

  TEST(Transformable, ShouldIgnoreNonTransformableParentInGlobalTransform) {
    // A non-Transformable parent (e.g. Scene) doesn't contribute to the
    // global transform — globalTransform should just return localTransform.
    // We can't add a Transformable to a bare Element (canHaveChild=false),
    // so use raw setParent to install the non-Transformable parent.
    Element parent;
    Transformable child;
    child.setParent(&parent);
    child.setPosition(Vector3d(1, 2, 3));
    ASSERT_MATRIX_NEAR(child.localTransform(), child.globalTransform(), 1e-9);
  }

  TEST(Transformable, ShouldRoundtripTranslationViaSetMatrix) {
    Transformable a;
    a.setPosition(Vector3d(1, 2, 3));

    Transformable b;
    b.setMatrix(a.localTransform());
    ASSERT_VECTOR_NEAR(Vector3d(1, 2, 3), b.position(), 1e-9);
  }

  TEST(Transformable, ShouldRoundtripScaleViaSetMatrix) {
    Transformable a;
    a.setScale(Vector3d(2, 3, 4));

    Transformable b;
    b.setMatrix(a.localTransform());
    ASSERT_VECTOR_NEAR(Vector3d(2, 3, 4), b.scale(), 1e-9);
  }

  TEST(Transformable, ShouldMoveByLocalVectorOnIdentityTransform) {
    Transformable t;
    t.moveBy(Vector3d(1, 2, 3));
    ASSERT_VECTOR_NEAR(Vector3d(1, 2, 3), t.position(), 1e-9);
  }

  TEST(Transformable, ShouldMoveByLocalVectorAccountsForScale) {
    Transformable t;
    t.setScale(Vector3d(2, 2, 2));
    // Local moveBy applies the local 3x3 transform (scale) to the offset
    // before adding to position — so a unit-x move on a 2x-scaled object
    // shifts position by (2, 0, 0).
    t.moveBy(Vector3d(1, 0, 0));
    ASSERT_VECTOR_NEAR(Vector3d(2, 0, 0), t.position(), 1e-9);
  }

  TEST(Transformable, ShouldMoveByGlobalVectorIgnoresLocalScaleWhenNoParent) {
    Transformable t;
    t.setScale(Vector3d(2, 2, 2));
    // global=true uses globalTransform.inverted() * localTransform; with
    // no parent the two are equal so the resulting matrix is identity and
    // the offset is (1,0,0) regardless of local scale.
    t.moveBy(Vector3d(1, 0, 0), /*global=*/true);
    ASSERT_VECTOR_NEAR(Vector3d(1, 0, 0), t.position(), 1e-9);
  }
}
