#include <gtest/gtest.h>

#include <memory>

#include <QByteArray>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "mcp/McpServer.h"
#include "mcp/SceneEditingTools.h"
#include "mcp/SceneEditor.h"

#include "widgets/world/SceneModel.h"

#include "world/objects/Box.h"
#include "world/objects/MatteMaterial.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"

#include "test/helpers/GuiTestHelper.h"

namespace SceneEditingToolsTest {

  namespace {
    QJsonObject requestObject(const QString& method, const QJsonObject& params, int id = 1) {
      QJsonObject request;
      request[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
      request[QStringLiteral("id")] = id;
      request[QStringLiteral("method")] = method;
      if (!params.isEmpty())
        request[QStringLiteral("params")] = params;
      return request;
    }

    QJsonObject toolCallParams(const QString& name, const QJsonObject& arguments) {
      QJsonObject params;
      params[QStringLiteral("name")] = name;
      if (!arguments.isEmpty())
        params[QStringLiteral("arguments")] = arguments;
      return params;
    }

    QJsonArray vec(double x, double y, double z) {
      return QJsonArray{x, y, z};
    }
  }

  class SceneEditingToolsTest : public ::testing::GuiTest {
  protected:
    void SetUp() override {
      ::testing::GuiTest::SetUp();
      scene = new Scene;
      model = std::make_unique<SceneModel>(scene);
      selectionModel = std::make_unique<QItemSelectionModel>(model.get());
      editor = std::make_unique<mcp::SceneEditor>([this]() -> Scene* { return scene; },
                                                  model.get(), selectionModel.get());
      server = std::make_unique<mcp::McpServer>([this]() -> Scene* { return scene; });
      mcp::registerSceneEditingTools(*server, *editor);
    }

    QJsonObject callTool(const QString& name, const QJsonObject& arguments) {
      const QJsonObject response = server->handleJsonRpcRequest(
        requestObject(QStringLiteral("tools/call"), toolCallParams(name, arguments)));
      return response[QStringLiteral("result")].toObject();
    }

    static QJsonObject resultData(const QJsonObject& toolResult) {
      const QJsonArray content = toolResult[QStringLiteral("content")].toArray();
      if (content.isEmpty())
        return QJsonObject();
      const QByteArray text = content[0].toObject()[QStringLiteral("text")].toString().toUtf8();
      return QJsonDocument::fromJson(text).object();
    }

    Scene* scene = nullptr;
    std::unique_ptr<SceneModel> model;
    std::unique_ptr<QItemSelectionModel> selectionModel;
    std::unique_ptr<mcp::SceneEditor> editor;
    std::unique_ptr<mcp::McpServer> server;
  };

  TEST_F(SceneEditingToolsTest, ToolsListAdvertisesAllNineEditingTools) {
    const QJsonObject response =
      server->handleJsonRpcRequest(requestObject(QStringLiteral("tools/list"), QJsonObject()));
    const QJsonArray tools =
      response[QStringLiteral("result")].toObject()[QStringLiteral("tools")].toArray();

    // query_scene (built in) + the 9 mutating tools registered here.
    ASSERT_EQ(10, tools.size());

    QStringList names;
    for (const auto& tool : tools)
      names << tool.toObject()[QStringLiteral("name")].toString();

    for (const QString& expected :
        {QStringLiteral("add_primitive"), QStringLiteral("transform"),
         QStringLiteral("apply_material"), QStringLiteral("select"), QStringLiteral("delete"),
         QStringLiteral("csg_union"), QStringLiteral("csg_intersect"),
         QStringLiteral("csg_difference"), QStringLiteral("set_camera")}) {
      EXPECT_TRUE(names.contains(expected)) << expected.toStdString();
    }
  }

  TEST_F(SceneEditingToolsTest, AddPrimitiveToolCreatesElementInLiveScene) {
    QJsonObject args;
    args[QStringLiteral("type")] = QStringLiteral("Sphere");
    args[QStringLiteral("position")] = vec(1, 2, 3);

    int elementChangedCount = 0;
    QObject::connect(editor.get(), &mcp::SceneEditor::elementChanged,
                     [&elementChangedCount](Element*) { ++elementChangedCount; });

    const QJsonObject toolResult = callTool(QStringLiteral("add_primitive"), args);
    EXPECT_FALSE(toolResult[QStringLiteral("isError")].toBool());

    const QJsonObject data = resultData(toolResult);
    EXPECT_TRUE(data[QStringLiteral("ok")].toBool());
    const QString id = data[QStringLiteral("id")].toString();
    ASSERT_FALSE(id.isEmpty());

    auto* sphere = qobject_cast<Sphere*>(scene->findById(id));
    ASSERT_NE(nullptr, sphere);
    EXPECT_EQ(Vector3d(1, 2, 3), sphere->position());
    EXPECT_EQ(1, elementChangedCount);
  }

  TEST_F(SceneEditingToolsTest, AddPrimitiveToolReportsErrorForUnsupportedType) {
    QJsonObject args;
    args[QStringLiteral("type")] = QStringLiteral("PortalMaterial");

    const QJsonObject toolResult = callTool(QStringLiteral("add_primitive"), args);
    EXPECT_TRUE(toolResult[QStringLiteral("isError")].toBool());
  }

  TEST_F(SceneEditingToolsTest, TransformToolMovesElementAndFiresElementChanged) {
    auto* box = new Box;
    scene->addChild(box);

    int elementChangedCount = 0;
    QObject::connect(editor.get(), &mcp::SceneEditor::elementChanged,
                     [&elementChangedCount](Element*) { ++elementChangedCount; });

    QJsonObject args;
    args[QStringLiteral("id")] = box->id();
    args[QStringLiteral("translate")] = vec(5, 0, 0);

    const QJsonObject toolResult = callTool(QStringLiteral("transform"), args);
    EXPECT_FALSE(toolResult[QStringLiteral("isError")].toBool());
    EXPECT_EQ(Vector3d(5, 0, 0), box->position());
    EXPECT_EQ(1, elementChangedCount);
  }

  TEST_F(SceneEditingToolsTest, ApplyMaterialToolAttachesExistingMaterial) {
    auto* box = new Box;
    auto* material = new MatteMaterial;
    scene->addChild(box);
    scene->addChild(material);

    QJsonObject args;
    args[QStringLiteral("id")] = box->id();
    args[QStringLiteral("material")] = material->id();

    const QJsonObject toolResult = callTool(QStringLiteral("apply_material"), args);
    EXPECT_FALSE(toolResult[QStringLiteral("isError")].toBool());
    EXPECT_EQ(material, box->material());
  }

  TEST_F(SceneEditingToolsTest, SelectToolDrivesSelectionModel) {
    auto* box = new Box;
    scene->addChild(box);

    int currentChangedCount = 0;
    QObject::connect(selectionModel.get(), &QItemSelectionModel::currentChanged,
                     [&currentChangedCount](const QModelIndex&, const QModelIndex&) {
                       ++currentChangedCount;
                     });

    QJsonObject args;
    args[QStringLiteral("id")] = box->id();

    const QJsonObject toolResult = callTool(QStringLiteral("select"), args);
    EXPECT_FALSE(toolResult[QStringLiteral("isError")].toBool());
    EXPECT_EQ(1, currentChangedCount);
    ASSERT_TRUE(selectionModel->currentIndex().isValid());
    EXPECT_EQ(box, static_cast<Element*>(selectionModel->currentIndex().internalPointer()));
  }

  TEST_F(SceneEditingToolsTest, DeleteToolRemovesElementAndFiresRowsRemoved) {
    auto* box = new Box;
    scene->addChild(box);
    const QString id = box->id();

    int rowsRemovedCount = 0;
    QObject::connect(model.get(), &SceneModel::rowsRemoved,
                     [&rowsRemovedCount](const QModelIndex&, int, int) { ++rowsRemovedCount; });

    QJsonObject args;
    args[QStringLiteral("id")] = id;

    const QJsonObject toolResult = callTool(QStringLiteral("delete"), args);
    EXPECT_FALSE(toolResult[QStringLiteral("isError")].toBool());
    EXPECT_EQ(1, rowsRemovedCount);
    EXPECT_EQ(nullptr, scene->findById(id));
  }

  TEST_F(SceneEditingToolsTest, CsgUnionToolReparentsOperandsAndFiresRowsInsertedForNewNode) {
    auto* box = new Box;
    auto* sphere = new Sphere;
    scene->addChild(box);
    scene->addChild(sphere);

    int rowsInsertedCount = 0;
    QObject::connect(model.get(), &SceneModel::rowsInserted,
                     [&rowsInsertedCount](const QModelIndex&, int, int) { ++rowsInsertedCount; });

    QJsonObject args;
    args[QStringLiteral("a")] = box->id();
    args[QStringLiteral("b")] = sphere->id();

    const QJsonObject toolResult = callTool(QStringLiteral("csg_union"), args);
    EXPECT_FALSE(toolResult[QStringLiteral("isError")].toBool());
    EXPECT_GE(rowsInsertedCount, 1);

    const QString unionId = resultData(toolResult)[QStringLiteral("id")].toString();
    Element* unionElement = scene->findById(unionId);
    ASSERT_NE(nullptr, unionElement);
    EXPECT_EQ(unionElement, box->parent());
    EXPECT_EQ(unionElement, sphere->parent());
  }

  TEST_F(SceneEditingToolsTest, CsgIntersectAndDifferenceToolsWork) {
    auto* box1 = new Box;
    auto* sphere1 = new Sphere;
    scene->addChild(box1);
    scene->addChild(sphere1);

    QJsonObject intersectArgs;
    intersectArgs[QStringLiteral("a")] = box1->id();
    intersectArgs[QStringLiteral("b")] = sphere1->id();
    const QJsonObject intersectResult = callTool(QStringLiteral("csg_intersect"), intersectArgs);
    EXPECT_FALSE(intersectResult[QStringLiteral("isError")].toBool());

    auto* box2 = new Box;
    auto* sphere2 = new Sphere;
    scene->addChild(box2);
    scene->addChild(sphere2);

    QJsonObject differenceArgs;
    differenceArgs[QStringLiteral("a")] = box2->id();
    differenceArgs[QStringLiteral("b")] = sphere2->id();
    const QJsonObject differenceResult = callTool(QStringLiteral("csg_difference"), differenceArgs);
    EXPECT_FALSE(differenceResult[QStringLiteral("isError")].toBool());
  }

  TEST_F(SceneEditingToolsTest, SetCameraToolUpdatesActiveCamera) {
    auto* camera = new PinholeCamera;
    scene->addChild(camera);

    int elementChangedCount = 0;
    QObject::connect(editor.get(), &mcp::SceneEditor::elementChanged,
                     [&elementChangedCount](Element*) { ++elementChangedCount; });

    QJsonObject args;
    args[QStringLiteral("position")] = vec(0, 1, 10);
    args[QStringLiteral("target")] = vec(0, 0, 0);

    const QJsonObject toolResult = callTool(QStringLiteral("set_camera"), args);
    EXPECT_FALSE(toolResult[QStringLiteral("isError")].toBool());
    EXPECT_EQ(Vector3d(0, 1, 10), camera->position());
    EXPECT_EQ(1, elementChangedCount);
  }

  TEST_F(SceneEditingToolsTest, ToolsRejectUnknownIds) {
    QJsonObject args;
    args[QStringLiteral("id")] = QStringLiteral("does-not-exist");
    args[QStringLiteral("translate")] = vec(1, 0, 0);

    const QJsonObject toolResult = callTool(QStringLiteral("transform"), args);
    EXPECT_TRUE(toolResult[QStringLiteral("isError")].toBool());
  }

}
