#include <gtest/gtest.h>

#include "world/objects/Element.h"
#include "world/objects/ElementFactory.h"
#include "world/objects/ConstantColorTexture.h"
#include "world/objects/Group.h"
#include "world/objects/MatteMaterial.h"
#include "world/objects/PointLight.h"
#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"
#include "core/Buffer.h"
#include "engine/raster/Rasterizer.h"
#include "engine/raytracer/Raytracer.h"
#include "engine/wireframe/Wireframe.h"
#include "render/cameras/PinholeCamera.h"
#include "render/primitives/Instance.h"
#include "render/primitives/Primitive.h"
#include "render/primitives/Scene.h"

#include <memory>
#include <QJsonArray>
#include <QJsonObject>

namespace GroupTest {
  namespace {
    int countPixels(const Buffer<Colord>& buffer, const Colord& color) {
      int count = 0;
      for (int y = 0; y < buffer.height(); ++y) {
        for (int x = 0; x < buffer.width(); ++x) {
          if (buffer[y][x] == color)
            ++count;
        }
      }
      return count;
    }

    int countNonBackground(const Buffer<Colord>& buffer, const Colord& background) {
      return buffer.width() * buffer.height() - countPixels(buffer, background);
    }

    int countLeaves(const render::Scene& scene) {
      int count = 0;
      scene.forEachLeaf([&](const render::Primitive*, std::shared_ptr<render::Material>) {
        ++count;
      });
      return count;
    }

    std::shared_ptr<render::PinholeCamera> camera() {
      return std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null);
    }

    std::unique_ptr<Scene>
    sceneWithNestedGroup(bool outerVisible, bool innerVisible, bool sphereVisible) {
      auto scene = std::make_unique<Scene>();
      scene->setAmbient(Colord::white());
      scene->setBackground(Colord::black());

      auto* material = new MatteMaterial;
      auto* texture = new ConstantColorTexture;
      texture->setColor(Colord::white());
      material->setDiffuseTexture(texture);
      scene->addChild(material);
      scene->addChild(texture);

      auto* outer = new Group;
      outer->setVisible(outerVisible);
      scene->addChild(outer);

      auto* inner = new Group;
      inner->setVisible(innerVisible);
      outer->addChild(inner);

      auto* sphere = new Sphere;
      sphere->setVisible(sphereVisible);
      sphere->setMaterial(material);
      inner->addChild(sphere);

      inner->addChild(new PointLight);

      return scene;
    }

    template<class Engine>
    int renderedNonBackgroundPixels(const std::shared_ptr<render::Scene>& scene) {
      auto engine = std::make_shared<Engine>(camera(), scene);
      Buffer<Colord> buffer(64, 64);
      engine->render(buffer);
      return countNonBackground(buffer, Colord::black());
    }
  }

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

  TEST(Group, ShouldHaveNoMetadataByDefault) {
    Group group;
    EXPECT_TRUE(group.metadata().isEmpty());

    QJsonObject json;
    group.write(json);

    EXPECT_FALSE(json.contains("metadata"));
  }

  TEST(Group, ShouldSetAndGetMetadataValues) {
    Group group;
    group.setMetadataValue("sourceFormat", "gltf");
    group.setMetadataValue("stepIndex", 3);
    group.setMetadataValue("category", QJsonArray{"chain", "link"});

    EXPECT_EQ(QString("gltf"), group.metadataValue("sourceFormat").toString());
    EXPECT_EQ(3, group.metadataValue("stepIndex").toInt());
    EXPECT_EQ(2, group.metadataValue("category").toArray().size());

    group.setMetadataValue("category", QJsonValue::Undefined);
    EXPECT_TRUE(group.metadataValue("category").isUndefined());
  }

  TEST(Group, ShouldRoundtripImporterMetadataThroughJson) {
    Group original;
    original.setMetadata(QJsonObject{
      {"sourceFormat", "step"},
      {"sourceId", "assembly/body/42"},
      {"stepIndex", 12},
      {"layerIndex", 4},
      {"chainId", "chain-A"},
      {"category", "fastener"},
      {"selected", true},
      {"tags", QJsonArray{"imported", "visible"}},
    });

    QJsonObject json;
    original.write(json);

    Group decoded;
    decoded.read(json);

    EXPECT_EQ(QString("step"), decoded.metadataValue("sourceFormat").toString());
    EXPECT_EQ(QString("assembly/body/42"), decoded.metadataValue("sourceId").toString());
    EXPECT_EQ(12, decoded.metadataValue("stepIndex").toInt());
    EXPECT_EQ(4, decoded.metadataValue("layerIndex").toInt());
    EXPECT_EQ(QString("chain-A"), decoded.metadataValue("chainId").toString());
    EXPECT_EQ(QString("fastener"), decoded.metadataValue("category").toString());
    EXPECT_TRUE(decoded.metadataValue("selected").toBool());
    EXPECT_EQ(QString("visible"), decoded.metadataValue("tags").toArray()[1].toString());
  }

  TEST(Group, ShouldPreserveUnknownMetadataTypesThroughSceneJson) {
    Scene original;
    auto* group = new Group;
    group->setMetadata(QJsonObject{
      {"unknownNumber", 1.25},
      {"unknownBoolean", false},
      {"unknownArray", QJsonArray{1, "two", true}},
      {"unknownObject", QJsonObject{{"nested", "value"}}},
    });
    original.addChild(group);

    QJsonObject json;
    original.write(json);

    Scene decoded;
    decoded.read(json);

    QJsonObject savedAgain;
    decoded.write(savedAgain);

    const auto savedMetadata =
      savedAgain["children"].toArray()[0].toObject()["metadata"].toObject();
    EXPECT_TRUE(savedMetadata["unknownNumber"].isDouble());
    EXPECT_DOUBLE_EQ(1.25, savedMetadata["unknownNumber"].toDouble());
    EXPECT_TRUE(savedMetadata["unknownBoolean"].isBool());
    EXPECT_FALSE(savedMetadata["unknownBoolean"].toBool());
    ASSERT_TRUE(savedMetadata["unknownArray"].isArray());
    EXPECT_EQ(QString("two"), savedMetadata["unknownArray"].toArray()[1].toString());
    ASSERT_TRUE(savedMetadata["unknownObject"].isObject());
    EXPECT_EQ(QString("value"),
              savedMetadata["unknownObject"].toObject()["nested"].toString());
  }

  TEST(Group, ShouldKeepMetadataOutOfRendering) {
    Group group;
    group.addChild(new Sphere);
    group.setMetadataValue("sourceFormat", "step");

    render::Scene scene;
    auto primitive = group.toRaytracer(&scene);
    ASSERT_NE(nullptr, primitive);
    EXPECT_EQ(Vector3d(-1, -1, -1), primitive->boundingBox().min());
    EXPECT_EQ(Vector3d(1, 1, 1), primitive->boundingBox().max());
    EXPECT_EQ(0u, scene.lights().size());
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

  TEST(Group, ShouldPreserveHiddenDescendantSurfaceWhenGroupIsVisible) {
    Group group;

    auto* childGroup = new Group;
    group.addChild(childGroup);

    auto* sphere = new Sphere;
    sphere->hide();
    childGroup->addChild(sphere);
    childGroup->addChild(new PointLight);

    render::Scene scene;
    EXPECT_EQ(nullptr, group.toRaytracer(&scene));
    EXPECT_EQ(1u, scene.lights().size());
  }

  TEST(Group, ShouldSuppressNestedDescendantSurfacesAndLightsWhenAncestorGroupIsInvisible) {
    Group group;
    group.hide();

    auto* childGroup = new Group;
    group.addChild(childGroup);
    childGroup->addChild(new Sphere);
    childGroup->addChild(new PointLight);

    render::Scene scene;
    EXPECT_EQ(nullptr, group.toRaytracer(&scene));
    EXPECT_EQ(0u, scene.lights().size());
  }

  TEST(Group, ShouldSuppressNestedDescendantSurfacesAndLightsWhenNestedGroupIsInvisible) {
    Group group;

    auto* childGroup = new Group;
    childGroup->hide();
    group.addChild(childGroup);
    childGroup->addChild(new Sphere);
    childGroup->addChild(new PointLight);

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

  TEST(Scene, ShouldComposeNestedGroupVisibilityInRaytracerSceneConversion) {
    auto hiddenOuter = sceneWithNestedGroup(false, true, true)->toRaytracerScene();
    EXPECT_EQ(0, countLeaves(*hiddenOuter));
    EXPECT_EQ(0u, hiddenOuter->lights().size());

    auto hiddenInner = sceneWithNestedGroup(true, false, true)->toRaytracerScene();
    EXPECT_EQ(0, countLeaves(*hiddenInner));
    EXPECT_EQ(0u, hiddenInner->lights().size());

    auto hiddenSurface = sceneWithNestedGroup(true, true, false)->toRaytracerScene();
    EXPECT_EQ(0, countLeaves(*hiddenSurface));
    EXPECT_EQ(1u, hiddenSurface->lights().size());

    auto visible = sceneWithNestedGroup(true, true, true)->toRaytracerScene();
    EXPECT_EQ(1, countLeaves(*visible));
    EXPECT_EQ(1u, visible->lights().size());
  }

  TEST(Scene, ShouldLoadNestedGroupTransformVisibilityFixture) {
    Scene scene;
    ASSERT_TRUE(scene.load("test/fixtures/groups/nested_transforms_visibility.json"));

    auto* rootCollection =
      dynamic_cast<Group*>(scene.findById("{90000000-0000-0000-0000-000000000100}"));
    ASSERT_NE(nullptr, rootCollection);
    EXPECT_EQ(QString("fixture"), rootCollection->metadataValue("sourceFormat").toString());
    EXPECT_EQ(QString("authoring-hierarchy"),
              rootCollection->metadataValue("layerName").toString());
    EXPECT_TRUE(rootCollection->visible());

    auto* visibleSubassembly =
      dynamic_cast<Group*>(scene.findById("{90000000-0000-0000-0000-000000000110}"));
    ASSERT_NE(nullptr, visibleSubassembly);
    EXPECT_EQ(1, visibleSubassembly->metadataValue("stepIndex").toInt());

    auto* hiddenSubassembly =
      dynamic_cast<Group*>(scene.findById("{90000000-0000-0000-0000-000000000120}"));
    ASSERT_NE(nullptr, hiddenSubassembly);
    EXPECT_FALSE(hiddenSubassembly->visible());

    auto rt = scene.toRaytracerScene();
    EXPECT_EQ(1, countLeaves(*rt));
    EXPECT_EQ(1u, rt->lights().size());
    ASSERT_EQ(1u, rt->primitives().size());
    EXPECT_EQ(Vector3d(2.0, 0.375, -0.25), rt->primitives()[0]->boundingBox().min());
    EXPECT_EQ(Vector3d(3.0, 0.625, 0.25), rt->primitives()[0]->boundingBox().max());
  }

  TEST(Scene, HiddenNestedGroupRendersAsBackgroundInRaytracerRasterizerAndWireframe) {
    auto rt = sceneWithNestedGroup(true, false, true)->toRaytracerScene();

    EXPECT_EQ(0, renderedNonBackgroundPixels<engine::raytracer::Raytracer>(rt));
    EXPECT_EQ(0, renderedNonBackgroundPixels<engine::raster::Rasterizer>(rt));
    EXPECT_EQ(0, renderedNonBackgroundPixels<engine::wireframe::Wireframe>(rt));
  }

  TEST(Scene, VisibleNestedGroupRendersGeometryInRaytracerRasterizerAndWireframe) {
    auto rt = sceneWithNestedGroup(true, true, true)->toRaytracerScene();

    EXPECT_GT(renderedNonBackgroundPixels<engine::raytracer::Raytracer>(rt), 0);
    EXPECT_GT(renderedNonBackgroundPixels<engine::raster::Rasterizer>(rt), 0);
    EXPECT_GT(renderedNonBackgroundPixels<engine::wireframe::Wireframe>(rt), 0);
  }
}
