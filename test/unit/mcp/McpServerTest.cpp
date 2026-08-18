#include <gtest/gtest.h>

#include <memory>

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include "mcp/McpServer.h"
#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"

#include "test/helpers/GuiTestHelper.h"

namespace McpServerTest {
  class McpServerTest : public ::testing::GuiTest {
  protected:
    static QJsonObject requestObject(const QString& method, const QJsonObject& params = QJsonObject(),
                                     int id = 1) {
      QJsonObject request;
      request[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
      request[QStringLiteral("id")] = id;
      request[QStringLiteral("method")] = method;
      if (!params.isEmpty())
        request[QStringLiteral("params")] = params;
      return request;
    }
  };

  TEST_F(McpServerTest, InitializeReturnsProtocolAndServerInfo) {
    mcp::McpServer server([]() -> Scene* { return nullptr; });

    const QJsonObject response =
      server.handleJsonRpcRequest(requestObject(QStringLiteral("initialize")));

    EXPECT_EQ(QStringLiteral("2.0"), response[QStringLiteral("jsonrpc")].toString());
    EXPECT_EQ(1, response[QStringLiteral("id")].toInt());

    const QJsonObject result = response[QStringLiteral("result")].toObject();
    EXPECT_EQ(QStringLiteral("2024-11-05"), result[QStringLiteral("protocolVersion")].toString());
    EXPECT_TRUE(
      result[QStringLiteral("capabilities")].toObject().contains(QStringLiteral("tools")));
    EXPECT_FALSE(result[QStringLiteral("serverInfo")]
                   .toObject()[QStringLiteral("name")]
                   .toString()
                   .isEmpty());
  }

  TEST_F(McpServerTest, NotificationsGetNoResponse) {
    mcp::McpServer server([]() -> Scene* { return nullptr; });

    QJsonObject notification;
    notification[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    notification[QStringLiteral("method")] = QStringLiteral("notifications/initialized");

    EXPECT_TRUE(server.handleJsonRpcRequest(notification).isEmpty());
  }

  TEST_F(McpServerTest, RequestWithoutIdForUnknownMethodGetsNoResponse) {
    mcp::McpServer server([]() -> Scene* { return nullptr; });

    QJsonObject request;
    request[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    request[QStringLiteral("method")] = QStringLiteral("whatever");

    EXPECT_TRUE(server.handleJsonRpcRequest(request).isEmpty());
  }

  TEST_F(McpServerTest, ToolsListAdvertisesQueryScene) {
    mcp::McpServer server([]() -> Scene* { return nullptr; });

    const QJsonObject response =
      server.handleJsonRpcRequest(requestObject(QStringLiteral("tools/list")));
    const QJsonArray tools =
      response[QStringLiteral("result")].toObject()[QStringLiteral("tools")].toArray();

    ASSERT_EQ(1, tools.size());
    EXPECT_EQ(QStringLiteral("query_scene"), tools[0].toObject()[QStringLiteral("name")].toString());
  }

  TEST_F(McpServerTest, ToolsCallQuerySceneReturnsSceneJsonAsText) {
    Scene scene;
    scene.setName(QStringLiteral("Fixture Scene"));
    auto sphere = std::make_unique<Sphere>();
    sphere->setId(QStringLiteral("sphere-1"));
    scene.addChild(std::move(sphere));

    mcp::McpServer server([&scene]() -> Scene* { return &scene; });

    QJsonObject params;
    params[QStringLiteral("name")] = QStringLiteral("query_scene");
    const QJsonObject response =
      server.handleJsonRpcRequest(requestObject(QStringLiteral("tools/call"), params));

    const QJsonObject result = response[QStringLiteral("result")].toObject();
    EXPECT_FALSE(result[QStringLiteral("isError")].toBool());

    const QJsonArray content = result[QStringLiteral("content")].toArray();
    ASSERT_EQ(1, content.size());
    const QJsonObject block = content[0].toObject();
    EXPECT_EQ(QStringLiteral("text"), block[QStringLiteral("type")].toString());

    const QJsonDocument sceneDoc =
      QJsonDocument::fromJson(block[QStringLiteral("text")].toString().toUtf8());
    ASSERT_TRUE(sceneDoc.isObject());
    EXPECT_EQ(QStringLiteral("Fixture Scene"), sceneDoc.object()[QStringLiteral("name")].toString());

    const QJsonArray children = sceneDoc.object()[QStringLiteral("children")].toArray();
    ASSERT_EQ(1, children.size());
    EXPECT_EQ(QStringLiteral("sphere-1"), children[0].toObject()[QStringLiteral("id")].toString());
  }

  TEST_F(McpServerTest, ToolsCallQuerySceneReportsErrorWhenNoSceneIsOpen) {
    mcp::McpServer server([]() -> Scene* { return nullptr; });

    QJsonObject params;
    params[QStringLiteral("name")] = QStringLiteral("query_scene");
    const QJsonObject response =
      server.handleJsonRpcRequest(requestObject(QStringLiteral("tools/call"), params));

    const QJsonObject result = response[QStringLiteral("result")].toObject();
    EXPECT_TRUE(result[QStringLiteral("isError")].toBool());
  }

  TEST_F(McpServerTest, ToolsCallRejectsUnknownTool) {
    mcp::McpServer server([]() -> Scene* { return nullptr; });

    QJsonObject params;
    params[QStringLiteral("name")] = QStringLiteral("delete_everything");
    const QJsonObject response =
      server.handleJsonRpcRequest(requestObject(QStringLiteral("tools/call"), params));

    EXPECT_EQ(-32602, response[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt());
  }

  TEST_F(McpServerTest, UnknownMethodReturnsMethodNotFound) {
    mcp::McpServer server([]() -> Scene* { return nullptr; });

    const QJsonObject response =
      server.handleJsonRpcRequest(requestObject(QStringLiteral("delete_scene")));

    EXPECT_EQ(-32601, response[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt());
  }

  TEST_F(McpServerTest, StartBindsLoopbackAndIssuesFreshTokenEachTime) {
    mcp::McpServer server([]() -> Scene* { return nullptr; });

    ASSERT_TRUE(server.start());
    EXPECT_TRUE(server.isRunning());
    EXPECT_EQ(QHostAddress(QHostAddress::LocalHost), server.address());
    EXPECT_NE(0, server.port());
    const QString firstToken = server.authToken();
    EXPECT_FALSE(firstToken.isEmpty());

    ASSERT_TRUE(server.start());
    EXPECT_NE(firstToken, server.authToken());

    server.stop();
    EXPECT_FALSE(server.isRunning());
    EXPECT_EQ(0, server.port());
  }
}
