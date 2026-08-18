#include "mcp/McpServer.h"

#include "mcp/QuerySceneTool.h"

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QList>
#include <QMap>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUuid>

namespace mcp {

  namespace {
    constexpr auto kJsonRpcVersion = "2.0";
    constexpr auto kProtocolVersion = "2024-11-05";
    constexpr auto kServerName = "raytracer-modeler";
    constexpr auto kServerVersion = "0.1.0";
    constexpr auto kQuerySceneTool = "query_scene";

    QString generateToken() {
      // Two random UUIDs concatenated (with the punctuation stripped) give a
      // 64-hex-character bearer token; there is no standard "random token"
      // Qt primitive, and QUuid::createUuid() is already the library's
      // source of cryptographically-strong randomness.
      return QUuid::createUuid().toString(QUuid::WithoutBraces).remove(QLatin1Char('-')) +
             QUuid::createUuid().toString(QUuid::WithoutBraces).remove(QLatin1Char('-'));
    }

    struct HttpRequest {
      QString method;
      QString path;
      QMap<QString, QString> query;
      QMap<QString, QString> headers;
      QByteArray body;
    };

    // Attempts to parse a complete HTTP request out of the front of
    // `buffer`. Returns false when more bytes are needed (the caller should
    // keep buffering and try again once more data arrives). On success,
    // `consumedBytes` is set to the number of bytes the request occupied so
    // the caller can drop them from the buffer (pipelining support).
    bool tryParseHttpRequest(const QByteArray& buffer, HttpRequest& request, int& consumedBytes) {
      const int headerEnd = buffer.indexOf("\r\n\r\n");
      if (headerEnd < 0)
        return false;

      const QByteArray headerBlob = buffer.left(headerEnd);
      QList<QByteArray> lines = headerBlob.split('\n');
      if (lines.isEmpty())
        return false;

      const QByteArray requestLine = lines.takeFirst().trimmed();
      const QList<QByteArray> requestLineParts = requestLine.split(' ');
      if (requestLineParts.size() < 2)
        return false;

      request.method = QString::fromLatin1(requestLineParts[0]);
      const QString target = QString::fromUtf8(requestLineParts[1]);
      const int queryPos = target.indexOf(QLatin1Char('?'));
      if (queryPos >= 0) {
        request.path = target.left(queryPos);
        const QString queryString = target.mid(queryPos + 1);
        const QStringList pairs = queryString.split(QLatin1Char('&'), Qt::SkipEmptyParts);
        for (const QString& pair : pairs) {
          const int equalsPos = pair.indexOf(QLatin1Char('='));
          if (equalsPos >= 0) {
            request.query[pair.left(equalsPos)] =
              QUrl::fromPercentEncoding(pair.mid(equalsPos + 1).toUtf8());
          } else {
            request.query[pair] = QString();
          }
        }
      } else {
        request.path = target;
      }

      for (const QByteArray& line : lines) {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.isEmpty())
          continue;
        const int colon = trimmed.indexOf(':');
        if (colon < 0)
          continue;
        const QString key = QString::fromLatin1(trimmed.left(colon)).trimmed().toLower();
        const QString value = QString::fromUtf8(trimmed.mid(colon + 1)).trimmed();
        request.headers[key] = value;
      }

      qint64 contentLength = 0;
      const auto contentLengthIt = request.headers.constFind(QStringLiteral("content-length"));
      if (contentLengthIt != request.headers.constEnd())
        contentLength = contentLengthIt.value().toLongLong();

      const int bodyStart = headerEnd + 4;
      if (buffer.size() - bodyStart < contentLength)
        return false;

      request.body = buffer.mid(bodyStart, contentLength);
      consumedBytes = bodyStart + static_cast<int>(contentLength);
      return true;
    }

    void writeHttpResponse(QTcpSocket* socket, int statusCode, const QString& statusText,
                           const QMap<QString, QString>& headers, const QByteArray& body) {
      QByteArray response =
        QStringLiteral("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText).toUtf8();
      for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
        response += QStringLiteral("%1: %2\r\n").arg(it.key(), it.value()).toUtf8();
      response += QStringLiteral("Content-Length: %1\r\n").arg(body.size()).toUtf8();
      response += QStringLiteral("Connection: close\r\n").toUtf8();
      response += "\r\n";
      response += body;
      socket->write(response);
    }

    void writeSseStreamHeaders(QTcpSocket* socket) {
      socket->write("HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/event-stream\r\n"
                    "Cache-Control: no-cache\r\n"
                    "Connection: keep-alive\r\n"
                    "\r\n");
    }

    void writeSseEvent(QTcpSocket* socket, const QString& event, const QByteArray& data) {
      QByteArray payload = "event: " + event.toUtf8() + "\n";
      const QList<QByteArray> dataLines = data.split('\n');
      for (const QByteArray& line : dataLines)
        payload += "data: " + line + "\n";
      payload += "\n";
      socket->write(payload);
    }
  }

  struct McpServer::Private {
    SceneProvider sceneProvider;
    QTcpServer* tcpServer = nullptr;
    QString authToken;

    QHash<QTcpSocket*, QByteArray> readBuffers;
    QHash<QString, QTcpSocket*> sseSocketsBySession;
    QHash<QTcpSocket*, QString> sessionsBySocket;

    bool isAuthorized(const HttpRequest& request) const {
      if (authToken.isEmpty())
        return false;

      const QString authHeader = request.headers.value(QStringLiteral("authorization"));
      static const QString bearerPrefix = QStringLiteral("Bearer ");
      if (authHeader.startsWith(bearerPrefix, Qt::CaseInsensitive) &&
          authHeader.mid(bearerPrefix.size()) == authToken) {
        return true;
      }

      return request.query.value(QStringLiteral("token")) == authToken;
    }

    void forgetSseSocket(QTcpSocket* socket) {
      const auto sessionIt = sessionsBySocket.find(socket);
      if (sessionIt != sessionsBySocket.end()) {
        sseSocketsBySession.remove(sessionIt.value());
        sessionsBySocket.erase(sessionIt);
      }
    }
  };

  McpServer::McpServer(SceneProvider sceneProvider, QObject* parent)
      : QObject(parent),
        p(std::make_unique<Private>()) {
    p->sceneProvider = std::move(sceneProvider);
  }

  McpServer::~McpServer() {
    stop();
  }

  bool McpServer::start() {
    stop();

    p->tcpServer = new QTcpServer(this);
    connect(p->tcpServer, &QTcpServer::newConnection, this, &McpServer::handleNewConnection);

    // Loopback only — never bind INADDR_ANY. Port 0 asks the OS for an
    // ephemeral free port.
    if (!p->tcpServer->listen(QHostAddress::LocalHost, 0)) {
      delete p->tcpServer;
      p->tcpServer = nullptr;
      return false;
    }

    p->authToken = generateToken();
    return true;
  }

  void McpServer::stop() {
    if (!p->tcpServer)
      return;

    const auto sockets = p->readBuffers.keys();
    for (QTcpSocket* socket : sockets) {
      socket->disconnect(this);
      socket->close();
      socket->deleteLater();
    }
    p->readBuffers.clear();
    p->sseSocketsBySession.clear();
    p->sessionsBySocket.clear();

    p->tcpServer->close();
    p->tcpServer->deleteLater();
    p->tcpServer = nullptr;
    p->authToken.clear();
  }

  bool McpServer::isRunning() const {
    return p->tcpServer && p->tcpServer->isListening();
  }

  quint16 McpServer::port() const {
    return isRunning() ? p->tcpServer->serverPort() : 0;
  }

  QHostAddress McpServer::address() const {
    return isRunning() ? p->tcpServer->serverAddress() : QHostAddress::Null;
  }

  QString McpServer::authToken() const {
    return p->authToken;
  }

  QString McpServer::sseUrl() const {
    if (!isRunning())
      return QString();
    return QStringLiteral("http://127.0.0.1:%1/sse").arg(port());
  }

  void McpServer::handleNewConnection() {
    while (p->tcpServer && p->tcpServer->hasPendingConnections()) {
      QTcpSocket* socket = p->tcpServer->nextPendingConnection();
      p->readBuffers.insert(socket, QByteArray());
      connect(socket, &QTcpSocket::readyRead, this, &McpServer::handleSocketReadyRead);
      connect(socket, &QTcpSocket::disconnected, this, &McpServer::handleSocketDisconnected);
    }
  }

  void McpServer::handleSocketReadyRead() {
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
      return;

    auto bufferIt = p->readBuffers.find(socket);
    if (bufferIt == p->readBuffers.end())
      return;

    bufferIt.value() += socket->readAll();

    HttpRequest request;
    int consumedBytes = 0;
    if (!tryParseHttpRequest(bufferIt.value(), request, consumedBytes))
      return;

    bufferIt.value().remove(0, consumedBytes);

    if (!p->isAuthorized(request)) {
      writeHttpResponse(socket, 401, QStringLiteral("Unauthorized"), {}, QByteArray());
      socket->disconnectFromHost();
      return;
    }

    if (request.method == QStringLiteral("GET") && request.path == QStringLiteral("/sse")) {
      const QString sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
      p->sseSocketsBySession.insert(sessionId, socket);
      p->sessionsBySocket.insert(socket, sessionId);
      writeSseStreamHeaders(socket);
      writeSseEvent(socket, QStringLiteral("endpoint"),
                    QStringLiteral("/message?sessionId=%1").arg(sessionId).toUtf8());
      return;
    }

    if (request.method == QStringLiteral("POST") && request.path == QStringLiteral("/message")) {
      QJsonParseError parseError{};
      const QJsonDocument doc = QJsonDocument::fromJson(request.body, &parseError);
      if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        writeHttpResponse(socket, 400, QStringLiteral("Bad Request"), {}, QByteArray());
        socket->disconnectFromHost();
        return;
      }

      const QJsonObject response = handleJsonRpcRequest(doc.object());

      writeHttpResponse(socket, 202, QStringLiteral("Accepted"), {}, QByteArray());
      socket->disconnectFromHost();

      if (!response.isEmpty()) {
        const QString sessionId = request.query.value(QStringLiteral("sessionId"));
        QTcpSocket* sseSocket = p->sseSocketsBySession.value(sessionId, nullptr);
        if (sseSocket) {
          const QByteArray data = QJsonDocument(response).toJson(QJsonDocument::Compact);
          writeSseEvent(sseSocket, QStringLiteral("message"), data);
        }
      }
      return;
    }

    writeHttpResponse(socket, 404, QStringLiteral("Not Found"), {}, QByteArray());
    socket->disconnectFromHost();
  }

  void McpServer::handleSocketDisconnected() {
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
      return;

    p->forgetSseSocket(socket);
    p->readBuffers.remove(socket);
    socket->deleteLater();
  }

  QJsonObject McpServer::handleJsonRpcRequest(const QJsonObject& request) const {
    const bool hasId = request.contains(QStringLiteral("id"));

    auto makeResponse = [&](const QString& key, const QJsonValue& value) {
      QJsonObject response;
      response[QStringLiteral("jsonrpc")] = QString::fromLatin1(kJsonRpcVersion);
      response[QStringLiteral("id")] = request.value(QStringLiteral("id"));
      response[key] = value;
      return response;
    };

    const QString method = request.value(QStringLiteral("method")).toString();

    if (method == QStringLiteral("initialize")) {
      if (!hasId)
        return QJsonObject();

      QJsonObject toolsCapability;
      QJsonObject capabilities;
      capabilities[QStringLiteral("tools")] = toolsCapability;

      QJsonObject serverInfo;
      serverInfo[QStringLiteral("name")] = QString::fromLatin1(kServerName);
      serverInfo[QStringLiteral("version")] = QString::fromLatin1(kServerVersion);

      QJsonObject result;
      result[QStringLiteral("protocolVersion")] = QString::fromLatin1(kProtocolVersion);
      result[QStringLiteral("capabilities")] = capabilities;
      result[QStringLiteral("serverInfo")] = serverInfo;
      return makeResponse(QStringLiteral("result"), result);
    }

    if (method == QStringLiteral("tools/list")) {
      if (!hasId)
        return QJsonObject();

      QJsonObject inputSchema;
      inputSchema[QStringLiteral("type")] = QStringLiteral("object");
      inputSchema[QStringLiteral("properties")] = QJsonObject();

      QJsonObject tool;
      tool[QStringLiteral("name")] = QString::fromLatin1(kQuerySceneTool);
      tool[QStringLiteral("description")] =
        QStringLiteral("Read-only dump of the current scene graph: element ids, types, "
                       "names, key parameters, and hierarchy.");
      tool[QStringLiteral("inputSchema")] = inputSchema;

      QJsonArray tools;
      tools.append(tool);

      QJsonObject result;
      result[QStringLiteral("tools")] = tools;
      return makeResponse(QStringLiteral("result"), result);
    }

    if (method == QStringLiteral("tools/call")) {
      if (!hasId)
        return QJsonObject();

      const QJsonObject params = request.value(QStringLiteral("params")).toObject();
      const QString toolName = params.value(QStringLiteral("name")).toString();

      if (toolName != QString::fromLatin1(kQuerySceneTool)) {
        QJsonObject error;
        error[QStringLiteral("code")] = -32602;
        error[QStringLiteral("message")] = QStringLiteral("Unknown tool: %1").arg(toolName);
        return makeResponse(QStringLiteral("error"), error);
      }

      Scene* scene = p->sceneProvider ? p->sceneProvider() : nullptr;

      QJsonObject contentBlock;
      contentBlock[QStringLiteral("type")] = QStringLiteral("text");
      bool isError = false;
      if (scene) {
        const QJsonObject sceneJson = querySceneToJson(*scene);
        contentBlock[QStringLiteral("text")] =
          QString::fromUtf8(QJsonDocument(sceneJson).toJson(QJsonDocument::Compact));
      } else {
        isError = true;
        contentBlock[QStringLiteral("text")] = QStringLiteral("No scene is currently open.");
      }

      QJsonArray content;
      content.append(contentBlock);

      QJsonObject result;
      result[QStringLiteral("content")] = content;
      result[QStringLiteral("isError")] = isError;
      return makeResponse(QStringLiteral("result"), result);
    }

    // Notifications (e.g. `notifications/initialized`) and any other
    // id-less message get no response.
    if (!hasId)
      return QJsonObject();

    QJsonObject error;
    error[QStringLiteral("code")] = -32601;
    error[QStringLiteral("message")] = QStringLiteral("Method not found");
    return makeResponse(QStringLiteral("error"), error);
  }

}
