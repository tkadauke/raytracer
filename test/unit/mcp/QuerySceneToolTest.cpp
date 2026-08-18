#include <gtest/gtest.h>

#include <memory>

#include <QJsonArray>
#include <QJsonObject>

#include "mcp/QuerySceneTool.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"

#include "test/helpers/GuiTestHelper.h"

namespace QuerySceneToolTest {
  class QuerySceneToolTest : public ::testing::GuiTest {};

  TEST_F(QuerySceneToolTest, ShouldReportSceneTypeAndName) {
    Scene scene;
    scene.setName("Demo Scene");

    const QJsonObject json = mcp::querySceneToJson(scene);

    EXPECT_EQ(QStringLiteral("Scene"), json[QStringLiteral("type")].toString());
    EXPECT_EQ(QStringLiteral("Demo Scene"), json[QStringLiteral("name")].toString());
  }

  TEST_F(QuerySceneToolTest, ShouldOmitChildrenKeyForAnEmptyScene) {
    Scene scene;
    const QJsonObject json = mcp::querySceneToJson(scene);
    EXPECT_FALSE(json.contains(QStringLiteral("children")));
  }

  TEST_F(QuerySceneToolTest, ShouldIncludeElementIdsTypesNamesAndHierarchy) {
    Scene scene;

    auto camera = std::make_unique<PinholeCamera>();
    camera->setId(QStringLiteral("cam-1"));
    camera->setName(QStringLiteral("Main Camera"));
    scene.addChild(std::move(camera));

    auto sphere = std::make_unique<Sphere>();
    sphere->setId(QStringLiteral("sphere-1"));
    sphere->setName(QStringLiteral("Ball"));
    sphere->setRadius(2.5);
    scene.addChild(std::move(sphere));

    const QJsonObject json = mcp::querySceneToJson(scene);
    const QJsonArray children = json[QStringLiteral("children")].toArray();
    ASSERT_EQ(2, children.size());

    const QJsonObject cameraJson = children[0].toObject();
    EXPECT_EQ(QStringLiteral("PinholeCamera"), cameraJson[QStringLiteral("type")].toString());
    EXPECT_EQ(QStringLiteral("cam-1"), cameraJson[QStringLiteral("id")].toString());
    EXPECT_EQ(QStringLiteral("Main Camera"), cameraJson[QStringLiteral("name")].toString());

    const QJsonObject sphereJson = children[1].toObject();
    EXPECT_EQ(QStringLiteral("Sphere"), sphereJson[QStringLiteral("type")].toString());
    EXPECT_EQ(QStringLiteral("sphere-1"), sphereJson[QStringLiteral("id")].toString());
    EXPECT_DOUBLE_EQ(2.5, sphereJson[QStringLiteral("radius")].toDouble());
  }
}
