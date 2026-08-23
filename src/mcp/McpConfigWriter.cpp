#include "mcp/McpConfigWriter.h"

#include "mcp/McpServer.h"

#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace mcp {

  namespace {
    constexpr auto kServerKey = "raytracer-modeler";
    constexpr auto kConfigFileName = "mcp-config.json";
  }

  QString writeMcpConfig(const McpServer& server) {
    if (!server.isRunning())
      return QString();

    const QString dirPath =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
      QStringLiteral("/mcp");
    if (!QDir().mkpath(dirPath))
      return QString();

    QJsonObject headers;
    headers[QStringLiteral("Authorization")] =
      QStringLiteral("Bearer %1").arg(server.authToken());

    QJsonObject serverEntry;
    serverEntry[QStringLiteral("type")] = QStringLiteral("sse");
    serverEntry[QStringLiteral("url")] = server.sseUrl();
    serverEntry[QStringLiteral("headers")] = headers;

    QJsonObject mcpServers;
    mcpServers[QString::fromLatin1(kServerKey)] = serverEntry;

    QJsonObject root;
    root[QStringLiteral("mcpServers")] = mcpServers;

    const QString filePath = dirPath + QLatin1Char('/') + QString::fromLatin1(kConfigFileName);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
      return QString();

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();

    // The file carries a live bearer token; keep it readable/writable by
    // the owner only.
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    return filePath;
  }

}
