#include <gtest/gtest.h>

#include "world/objects/Element.h"
#include "world/objects/ElementFactory.h"
#include "world/objects/Group.h"
#include "world/objects/PointLight.h"
#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"
#include "render/primitives/Instance.h"
#include "render/primitives/Primitive.h"
#include "render/primitives/Scene.h"

namespace GroupTest {
  TEST(Group, ShouldDefaultToVisible) {
    Group group;
    EXPECT_TRUE(group.visible());
  }

  TEST(Group, ShouldSetAndGetVisible) {
    Group group;
    group.setVisible(false);
    EXPECT_FALSE(group.visible());
  }

  TEST(Group, ShouldHideAfterShow) {
    Group group;
    group.show();
    group.hide();
    EXPECT_FALSE(group.visible());
  }

  TEST(Group, ShouldShowAfterHide) {
    Group group;
    group.hide();
    group.show();
    EXPECT_TRUE(group.visible());
  }

  TEST(Group, ShouldAllowSurfaceLightAndGroupChildren) {
    Group group;
    Sphere surface;
    PointLight light;
    Group childGroup;

    EXPECT_TRUE(group.canHaveChild(&surface));
    EXPECT_TRUE(group.canHaveChild(&light));
    EXPECT_TRUE(group.canHaveChild(&childGroup));
  }

  TEST(Group, ShouldRejectOtherChildren) {
    Group group;
    Element other;

    EXPECT_FALSE(group.canHaveChild(&other));
  }

  TEST(Group, ShouldBeAcceptedBySurfaceParents) {
    Sphere surface;
    Group group;

    EXPECT_TRUE(surface.canHaveChild(&group));
  }

  TEST(Group, ShouldBeRegisteredWithElementFactory) {
    auto group = ElementFactory::self().create("Group");
    ASSERT_NE(nullptr, group);
    EXPECT_NE(nullptr, dynamic_cast<Group*>(group.get()));
  }

  TEST(Group, ShouldRegisterCollectionAliasWithElementFactory) {
    auto group = ElementFactory::self().create("Collection");
    ASSERT_NE(nullptr, group);
    EXPECT_NE(nullptr, dynamic_cast<Group*>(group.get()));
  }

  TEST(Group, ShouldReturnNullPrimitiveWhenEmpty) {
    Group group;
    render::Scene scene;

    EXPECT_EQ(nullptr, group.toRaytracer(&scene));
  }

  TEST(Group, ShouldConvertSurfaceChildrenIntoTransformedComposite) {
    Group group;
    group.setPosition(Vector3d(2, 0, 0));

    auto* sphere = new Sphere;
    group.addChild(sphere);
    sphere->setPosition(Vector3d(3, 0, 0));

    render::Scene scene;
    auto primitive = group.toRaytracer(&scene);

    ASSERT_NE(nullptr, primitive);
    EXPECT_NE(nullptr, std::dynamic_pointer_cast<render::Instance>(primitive));
    EXPECT_EQ(Vector3d(4, -1, -1), primitive->boundingBox().min());
    EXPECT_EQ(Vector3d(6, 1, 1), primitive->boundingBox().max());
  }

  TEST(Group, ShouldConvertNestedGroupChildren) {
    Group group;
    group.setPosition(Vector3d(2, 0, 0));

    auto* childGroup = new Group;
    group.addChild(childGroup);
    childGroup->addChild(new Sphere);
    childGroup->setPosition(Vector3d(3, 0, 0));

    render::Scene scene;
    auto primitive = group.toRaytracer(&scene);

    ASSERT_NE(nullptr, primitive);
    EXPECT_EQ(Vector3d(4, -1, -1), primitive->boundingBox().min());
    EXPECT_EQ(Vector3d(6, 1, 1), primitive->boundingBox().max());
  }

  TEST(Group, ShouldConvertWhenNestedUnderSurface) {
    Sphere parent;

    auto* group = new Group;
    parent.addChild(group);
    group->addChild(new Sphere);
    parent.setPosition(Vector3d(2, 0, 0));
    group->setPosition(Vector3d(3, 0, 0));

    render::Scene scene;
    auto primitive = parent.toRaytracer(&scene);

    ASSERT_NE(nullptr, primitive);
    EXPECT_EQ(Vector3d(1, -1, -1), primitive->boundingBox().min());
    EXPECT_EQ(Vector3d(6, 1, 1), primitive->boundingBox().max());
  }

  TEST(Group, ShouldAddVisibleChildLightsToRuntimeScene) {
    Group group;
    group.addChild(new PointLight);

    render::Scene scene;
    EXPECT_EQ(nullptr, group.toRaytracer(&scene));

    EXPECT_EQ(1u, scene.lights().size());
  }

  TEST(Group, ShouldSkipInvisibleChildSurfacesAndLights) {
    Group group;

    auto* sphere = new Sphere;
    sphere->hide();
    group.addChild(sphere);

    auto* light = new PointLight;
    light->hide();
    group.addChild(light);

    render::Scene scene;
    EXPECT_EQ(nullptr, group.toRaytracer(&scene));
    EXPECT_EQ(0u, scene.lights().size());
  }

  TEST(Group, ShouldSkipAllChildrenWhenInvisible) {
    Group group;
    group.hide();
    group.addChild(new Sphere);
    group.addChild(new PointLight);

    render::Scene scene;
    EXPECT_EQ(nullptr, group.toRaytracer(&scene));
    EXPECT_EQ(0u, scene.lights().size());
  }

  TEST(Scene, ShouldIncludeVisibleTopLevelGroupGeometry) {
    Scene scene;

    auto* group = new Group;
    group->addChild(new Sphere);
    scene.addChild(group);

    auto rt = scene.toRaytracerScene();

    EXPECT_EQ(1u, rt->primitives().size());
  }

  TEST(Scene, ShouldSkipInvisibleTopLevelGroup) {
    Scene scene;

    auto* group = new Group;
    group->hide();
    group->addChild(new Sphere);
    group->addChild(new PointLight);
    scene.addChild(group);

    auto rt = scene.toRaytracerScene();

    EXPECT_EQ(0u, rt->primitives().size());
    EXPECT_EQ(0u, rt->lights().size());
  }
}
