#include <gtest/gtest.h>

#include "world/import/ImportResult.h"
#include "world/objects/Element.h"
#include "world/objects/ElementFactory.h"
#include "world/objects/ConstantColorTexture.h"
#include "world/objects/Group.h"
#include "world/objects/MatteMaterial.h"
#include "world/objects/PointLight.h"
#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"
#include "world/objects/StepVisibilityEvaluator.h"
#include "core/Buffer.h"
#include "engine/raster/Rasterizer.h"
#include "engine/raytracer/Raytracer.h"
#include "engine/wireframe/Wireframe.h"
#include "render/cameras/PinholeCamera.h"
#include "render/primitives/Instance.h"
#include "render/primitives/Primitive.h"
#include "render/primitives/Scene.h"
#include "test/helpers/BufferTestHelper.h"
#include "test/helpers/CameraTestHelper.h"

#include <memory>
#include <QJsonArray>
#include <QJsonObject>

namespace GroupTest {
  using test::helpers::standardCamera;

  namespace {
    int countLeaves(const render::Scene& scene) {
      int count = 0;
      scene.forEachLeaf([&](const render::Primitive*, std::shared_ptr<render::Material>) {
        ++count;
      });
      return count;
    }

    int countLeavesWithMaterial(const render::Scene& scene) {
      int count = 0;
      scene.forEachLeaf([&](const render::Primitive*, std::shared_ptr<render::Material> material) {
        if (material)
          ++count;
      });
      return count;
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
      auto engine = std::make_shared<Engine>(standardCamera(), scene);
      Buffer<Colord> buffer(64, 64);
      engine->render(buffer);
      return test::helpers::countPixelsNotEqualTo(buffer, Colord::black());
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

  TEST(Group, ShouldSetGenericStepTimeAndLabelMetadata) {
    Group group;
    group.setMetadataValue(GroupMetadata::sourceFormatKey(), "imported-volume");
    group.setMetadataValue(GroupMetadata::sourceIdKey(), "slice-stack/42");
    group.setStepIndex(7);
    group.setLayerIndex(3);
    group.setTimeRange(1.25, 1.5);
    group.setLabel(QString("cooldown"));

    ASSERT_TRUE(group.stepIndex().has_value());
    EXPECT_EQ(7, *group.stepIndex());
    ASSERT_TRUE(group.layerIndex().has_value());
    EXPECT_EQ(3, *group.layerIndex());
    ASSERT_TRUE(group.startTime().has_value());
    EXPECT_DOUBLE_EQ(1.25, *group.startTime());
    ASSERT_TRUE(group.endTime().has_value());
    EXPECT_DOUBLE_EQ(1.5, *group.endTime());
    ASSERT_TRUE(group.label().has_value());
    EXPECT_EQ(QString("cooldown"), *group.label());
    EXPECT_EQ(QString("imported-volume"),
              group.metadataValue(GroupMetadata::sourceFormatKey()).toString());
    EXPECT_EQ(QString("slice-stack/42"),
              group.metadataValue(GroupMetadata::sourceIdKey()).toString());

    group.setStepIndex(std::nullopt);
    group.setLayerIndex(std::nullopt);
    group.setTimeRange(std::nullopt, std::nullopt);
    group.setLabel(std::nullopt);

    EXPECT_FALSE(group.stepIndex().has_value());
    EXPECT_FALSE(group.layerIndex().has_value());
    EXPECT_FALSE(group.startTime().has_value());
    EXPECT_FALSE(group.endTime().has_value());
    EXPECT_FALSE(group.label().has_value());
    EXPECT_TRUE(group.metadataValue(GroupMetadata::stepIndexKey()).isUndefined());
    EXPECT_TRUE(group.metadataValue(GroupMetadata::layerIndexKey()).isUndefined());
    EXPECT_TRUE(group.metadataValue(GroupMetadata::startTimeKey()).isUndefined());
    EXPECT_TRUE(group.metadataValue(GroupMetadata::endTimeKey()).isUndefined());
    EXPECT_TRUE(group.metadataValue(GroupMetadata::labelKey()).isUndefined());
  }

  TEST(Group, ShouldTreatMissingOrWrongTypedGenericMetadataAsAbsent) {
    Group missing;
    EXPECT_FALSE(missing.stepIndex().has_value());
    EXPECT_FALSE(missing.layerIndex().has_value());
    EXPECT_FALSE(missing.startTime().has_value());
    EXPECT_FALSE(missing.endTime().has_value());
    EXPECT_FALSE(missing.label().has_value());

    Group malformed;
    malformed.setMetadata(QJsonObject{
      {GroupMetadata::stepIndexKey(), 1.5},
      {GroupMetadata::layerIndexKey(), "four"},
      {GroupMetadata::startTimeKey(), "0.0"},
      {GroupMetadata::endTimeKey(), true},
      {GroupMetadata::labelKey(), 12},
    });

    EXPECT_FALSE(malformed.stepIndex().has_value());
    EXPECT_FALSE(malformed.layerIndex().has_value());
    EXPECT_FALSE(malformed.startTime().has_value());
    EXPECT_FALSE(malformed.endTime().has_value());
    EXPECT_FALSE(malformed.label().has_value());
  }

  TEST(Group, ShouldRoundtripGenericStepTimeMetadataThroughNestedSceneJson) {
    Scene original;

    auto* buildStep = new Group;
    buildStep->setId("{91000000-0000-0000-0000-000000000100}");
    buildStep->setStepIndex(2);
    buildStep->setTimeRange(10.0, 12.5);
    buildStep->setLabel(QString("support setup"));
    original.addChild(buildStep);

    auto* layer = new Group;
    layer->setId("{91000000-0000-0000-0000-000000000110}");
    layer->setLayerIndex(14);
    layer->setTimeRange(12.5, 13.0);
    layer->setLabel(QString("layer 14"));
    buildStep->addChild(layer);

    QJsonObject json;
    original.write(json);

    const auto children = json["children"].toArray();
    ASSERT_EQ(1, children.size());
    const auto parentMetadata = children[0].toObject()["metadata"].toObject();
    EXPECT_EQ(2, parentMetadata[GroupMetadata::stepIndexKey()].toInt());
    EXPECT_DOUBLE_EQ(10.0, parentMetadata[GroupMetadata::startTimeKey()].toDouble());
    EXPECT_DOUBLE_EQ(12.5, parentMetadata[GroupMetadata::endTimeKey()].toDouble());
    EXPECT_EQ(QString("support setup"), parentMetadata[GroupMetadata::labelKey()].toString());

    const auto nestedChildren = children[0].toObject()["children"].toArray();
    ASSERT_EQ(1, nestedChildren.size());
    const auto childMetadata = nestedChildren[0].toObject()["metadata"].toObject();
    EXPECT_EQ(14, childMetadata[GroupMetadata::layerIndexKey()].toInt());
    EXPECT_DOUBLE_EQ(12.5, childMetadata[GroupMetadata::startTimeKey()].toDouble());
    EXPECT_DOUBLE_EQ(13.0, childMetadata[GroupMetadata::endTimeKey()].toDouble());
    EXPECT_EQ(QString("layer 14"), childMetadata[GroupMetadata::labelKey()].toString());

    Scene decoded;
    decoded.read(json);

    auto* decodedStep =
      dynamic_cast<Group*>(decoded.findById("{91000000-0000-0000-0000-000000000100}"));
    ASSERT_NE(nullptr, decodedStep);
    ASSERT_TRUE(decodedStep->stepIndex().has_value());
    EXPECT_EQ(2, *decodedStep->stepIndex());
    ASSERT_TRUE(decodedStep->startTime().has_value());
    EXPECT_DOUBLE_EQ(10.0, *decodedStep->startTime());
    ASSERT_TRUE(decodedStep->endTime().has_value());
    EXPECT_DOUBLE_EQ(12.5, *decodedStep->endTime());
    ASSERT_TRUE(decodedStep->label().has_value());
    EXPECT_EQ(QString("support setup"), *decodedStep->label());

    auto* decodedLayer =
      dynamic_cast<Group*>(decoded.findById("{91000000-0000-0000-0000-000000000110}"));
    ASSERT_NE(nullptr, decodedLayer);
    ASSERT_TRUE(decodedLayer->layerIndex().has_value());
    EXPECT_EQ(14, *decodedLayer->layerIndex());
    ASSERT_TRUE(decodedLayer->startTime().has_value());
    EXPECT_DOUBLE_EQ(12.5, *decodedLayer->startTime());
    ASSERT_TRUE(decodedLayer->endTime().has_value());
    EXPECT_DOUBLE_EQ(13.0, *decodedLayer->endTime());
    ASSERT_TRUE(decodedLayer->label().has_value());
    EXPECT_EQ(QString("layer 14"), *decodedLayer->label());
  }

  TEST(Element, ShouldRoundtripObjectProvenanceMetadataThroughSceneJson) {
    Scene original;
    auto* sphere = new Sphere;
    sphere->setId("{90000000-0000-0000-0000-00000000f001}");

    world::ImportProvenance provenance;
    provenance.sourceFile = "assembly.step";
    provenance.sourceId = "assembly/body/42";
    provenance.lineStart = 120;
    provenance.lineEnd = 148;
    provenance.recordId = "#42";
    provenance.originalUnits = "mm";
    provenance.category = QJsonObject{{"kind", "solid"}, {"layer", "fasteners"}};
    world::setImportProvenance(*sphere, provenance);

    original.addChild(sphere);

    QJsonObject json;
    original.write(json);

    Scene decoded;
    decoded.read(json);

    auto* decodedSphere =
      dynamic_cast<Sphere*>(decoded.findById("{90000000-0000-0000-0000-00000000f001}"));
    ASSERT_NE(nullptr, decodedSphere);

    const auto decodedProvenance = world::importProvenance(*decodedSphere);
    ASSERT_TRUE(decodedProvenance.has_value());
    EXPECT_EQ(QString("assembly.step"), decodedProvenance->sourceFile);
    EXPECT_EQ(QString("assembly/body/42"), decodedProvenance->sourceId);
    EXPECT_EQ(120, decodedProvenance->lineStart);
    EXPECT_EQ(148, decodedProvenance->lineEnd);
    EXPECT_EQ(QString("#42"), decodedProvenance->recordId);
    EXPECT_EQ(QString("mm"), decodedProvenance->originalUnits);
    EXPECT_EQ(QString("solid"), decodedProvenance->category["kind"].toString());
    EXPECT_EQ(QString("fasteners"), decodedProvenance->category["layer"].toString());
  }

  TEST(Element, ShouldOmitEmptyImportProvenanceMetadata) {
    Sphere sphere;

    world::setImportProvenance(sphere, world::ImportProvenance());

    EXPECT_FALSE(world::importProvenance(sphere).has_value());
    EXPECT_TRUE(sphere.metadata().isEmpty());
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

  TEST(Scene, StepPlaybackHighlightOverridesActiveGroupMaterial) {
    Scene scene;

    auto* group = new Group;
    group->setStepIndex(2);
    group->addChild(new Sphere);
    scene.addChild(group);

    StepPlaybackStyle style;
    style.activeStep = 2;
    style.highlightActive = true;

    auto rt = scene.toRaytracerScene(style);

    EXPECT_EQ(1, countLeaves(*rt));
    EXPECT_EQ(1, countLeavesWithMaterial(*rt));
  }

  TEST(Scene, StepPlaybackGhostsPreviousGroupsAndHidesFutureGroups) {
    Scene scene;

    auto* previous = new Group;
    previous->setStepIndex(1);
    previous->addChild(new Sphere);
    scene.addChild(previous);

    auto* active = new Group;
    active->setStepIndex(2);
    active->addChild(new Sphere);
    scene.addChild(active);

    auto* future = new Group;
    future->setStepIndex(3);
    future->addChild(new Sphere);
    scene.addChild(future);

    StepPlaybackStyle style;
    style.activeStep = 2;
    style.highlightActive = true;
    style.ghostPrevious = true;

    auto rt = scene.toRaytracerScene(style);

    EXPECT_EQ(2, countLeaves(*rt));
    EXPECT_EQ(2, countLeavesWithMaterial(*rt));
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
    const auto primitive = rt->primitives().front();
    EXPECT_EQ(Vector3d(2.0, 0.375, -0.25), primitive->boundingBox().min());
    EXPECT_EQ(Vector3d(3.0, 0.625, 0.25), primitive->boundingBox().max());
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
