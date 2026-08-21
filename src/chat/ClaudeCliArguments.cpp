#include "chat/ClaudeCliArguments.h"

namespace chat {

  QStringList claudeCliArguments(const ClaudeCliRequest& request) {
    QStringList arguments;
    arguments << QStringLiteral("-p") << request.message << QStringLiteral("--output-format")
              << QStringLiteral("stream-json") << QStringLiteral("--input-format")
              << QStringLiteral("stream-json") << QStringLiteral("--verbose");

    if (!request.mcpConfigPath.isEmpty())
      arguments << QStringLiteral("--mcp-config") << request.mcpConfigPath;

    if (!request.resumeSessionId.isEmpty())
      arguments << QStringLiteral("--resume") << request.resumeSessionId;

    return arguments;
  }

}
