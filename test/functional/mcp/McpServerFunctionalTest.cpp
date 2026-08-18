#include <gtest/gtest.h>

#include <memory>

#include <QByteArray>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QTcpSocket>

#include "mcp/McpServer.h"
#include "world/objects/Scene.h"

#include "test/helpers/GuiTestHelper.h"

namespace McpServerFunctionalTest {

  namespace {
    // Reads from `socket` until `marker` shows up in the accumulated bytes
    // (or the timeout elapses), returning everything read so far. The real
    // HTTP+SSE framing is exercised end to end here, so responses can be
    // split across several TCP reads.
    QByteArray readUntil(QTcpSocket& socket, const QByteArray& marker, int timeoutMs = 3000) {
      QByteArray buffer;
      QElapsedTimer timer;
      timer.start();
      while (!buffer.contains(marker) && timer.elapsed() < timeoutMs) {
        if (socket.bytesAvailable() == 0)
          socket.waitForReadyRead(100);
        buffer += socket.readAll();
      }
      return buffer;
    }

    // Extracts the value of an SSE `field: value` line (e.g. `data: ...`)
    // from a raw SSE byte buffer.
    QByteArray extractSseField(const QByteArray& buffer, const QByteArray& field) {
      const QByteArray marker = field + ": ";
      const int pos = buffer.indexOf(marker);
      if (pos < 0)
        return QByteArray();

      const int valueStart = pos + marker.size();
      int valueEnd = buffer.indexOf('\n', valueStart);
      if (valueEnd < 0)
        valueEnd = buffer.size();
      return buffer.mid(valueStart, valueEnd - valueStart).trimmed();
    }
  }

  class McpServerFunctionalTest : public ::testing::GuiTest {
  protected:
    void SetUp() override {
      ::testing::GuiTest::SetUp();
      scene = std::make_unique<Scene>();
      scene->setName(QStringLiteral("Functional Fixture Scene"));
      server = std::make_unique<mcp::McpServer>([this]() -> Scene* { return scene.get(); });
      ASSERT_TRUE(server->start());
    }

    void TearDown() override {
      server->stop();
      ::testing::GuiTest::TearDown();
    }

    std::unique_ptr<Scene> scene;
    std::unique_ptr<mcp::McpServer> server;
  };

  TEST_F(McpServerFunctionalTest, BindsLoopbackOnlyOnAnEphemeralPort) {
    EXPECT_EQ(QHostAddress(QHostAddress::LocalHost), server->address());
    EXPECT_NE(0, server->port());
  }

  TEST_F(McpServerFunctionalTest, SseConnectionReceivesAnEndpointEvent) {
    QTcpSocket socket;
    socket.connectToHost(server->address(), server->port());
    ASSERT_TRUE(socket.waitForConnected(2000));

    const QByteArray request = QStringLiteral("GET /sse HTTP/1.1\r\n"
                                              "Host: 127.0.0.1\r\n"
                                              "Authorization: Bearer %1\r\n"
                                              "\r\n")
                                 .arg(server->authToken())
                                 .toUtf8();
    socket.write(request);
    ASSERT_TRUE(socket.waitForBytesWritten(2000));

    const QByteArray response = readUntil(socket, QByteArrayLiteral("data: /message?sessionId="));
    EXPECT_TRUE(response.contains("HTTP/1.1 200 OK"));
    EXPECT_TRUE(response.contains("text/event-stream"));
    EXPECT_TRUE(response.contains("event: endpoint"));
    EXPECT_TRUE(response.contains("data: /message?sessionId="));
  }

  TEST_F(McpServerFunctionalTest, RejectsRequestsWithoutAValidBearerToken) {
    QTcpSocket socket;
    socket.connectToHost(server->address(), server->port());
    ASSERT_TRUE(socket.waitForConnected(2000));

    socket.write("GET /sse HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
    ASSERT_TRUE(socket.waitForBytesWritten(2000));

    const QByteArray response = readUntil(socket, QByteArrayLiteral("401"));
    EXPECT_TRUE(response.contains("401"));
  }

  TEST_F(McpServerFunctionalTest, PostMessageDispatchesJsonRpcAndPushesResponseOverSse) {
    QTcpSocket sseSocket;
    sseSocket.connectToHost(server->address(), server->port());
    ASSERT_TRUE(sseSocket.waitForConnected(2000));
    sseSocket.write(QStringLiteral("GET /sse HTTP/1.1\r\n"
                                   "Host: 127.0.0.1\r\n"
                                   "Authorization: Bearer %1\r\n"
                                   "\r\n")
                      .arg(server->authToken())
                      .toUtf8());
    ASSERT_TRUE(sseSocket.waitForBytesWritten(2000));

    const QByteArray sseHeader =
      readUntil(sseSocket, QByteArrayLiteral("data: /message?sessionId="));
    const QByteArray endpointPath = extractSseField(sseHeader, QByteArrayLiteral("data"));
    ASSERT_FALSE(endpointPath.isEmpty());

    QJsonObject requestJson;
    requestJson[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    requestJson[QStringLiteral("id")] = 7;
    requestJson[QStringLiteral("method")] = QStringLiteral("initialize");
    const QByteArray body = QJsonDocument(requestJson).toJson(QJsonDocument::Compact);

    QTcpSocket postSocket;
    postSocket.connectToHost(server->address(), server->port());
    ASSERT_TRUE(postSocket.waitForConnected(2000));

    const QByteArray postRequest = QStringLiteral("POST %1 HTTP/1.1\r\n"
                                                  "Host: 127.0.0.1\r\n"
                                                  "Authorization: Bearer %2\r\n"
                                                  "Content-Type: application/json\r\n"
                                                  "Content-Length: %3\r\n"
                                                  "\r\n")
                                     .arg(QString::fromUtf8(endpointPath))
                                     .arg(server->authToken())
                                     .arg(body.size())
                                     .toUtf8() +
                                   body;
    postSocket.write(postRequest);
    ASSERT_TRUE(postSocket.waitForBytesWritten(2000));

    const QByteArray postResponse = readUntil(postSocket, QByteArrayLiteral("202"));
    EXPECT_TRUE(postResponse.contains("202"));

    const QByteArray sseMessage = readUntil(sseSocket, QByteArrayLiteral("event: message"));
    ASSERT_TRUE(sseMessage.contains("event: message"));

    const QByteArray messageTail = sseMessage.mid(sseMessage.indexOf("event: message"));
    const QByteArray jsonLine = extractSseField(messageTail, QByteArrayLiteral("data"));
    ASSERT_FALSE(jsonLine.isEmpty());

    const QJsonDocument doc = QJsonDocument::fromJson(jsonLine);
    ASSERT_TRUE(doc.isObject());
    EXPECT_EQ(7, doc.object()[QStringLiteral("id")].toInt());
    EXPECT_EQ(QStringLiteral("2024-11-05"), doc.object()[QStringLiteral("result")]
                                              .toObject()[QStringLiteral("protocolVersion")]
                                              .toString());
  }

}
