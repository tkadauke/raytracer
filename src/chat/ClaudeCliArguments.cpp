#include "chat/ClaudeCliArguments.h"

namespace chat {

  namespace {
    // Mirrors mcp::McpConfigWriter's server key ("raytracer-modeler") and the
    // tool names registered in mcp::QuerySceneTool / mcp::SceneEditingTools.
    // The CLI addresses MCP tools as `mcp__<server key>__<tool name>`.
    const QStringList& allowedMcpToolNames() {
      static const QStringList names{
        QStringLiteral("mcp__raytracer-modeler__query_scene"),
        QStringLiteral("mcp__raytracer-modeler__add_primitive"),
        QStringLiteral("mcp__raytracer-modeler__transform"),
        QStringLiteral("mcp__raytracer-modeler__apply_material"),
        QStringLiteral("mcp__raytracer-modeler__select"),
        QStringLiteral("mcp__raytracer-modeler__delete"),
        QStringLiteral("mcp__raytracer-modeler__csg_union"),
        QStringLiteral("mcp__raytracer-modeler__csg_intersect"),
        QStringLiteral("mcp__raytracer-modeler__csg_difference"),
        QStringLiteral("mcp__raytracer-modeler__set_camera"),
      };
      return names;
    }
  }

  QStringList claudeCliArguments(const ClaudeCliRequest& request) {
    QStringList arguments;
    arguments << QStringLiteral("-p") << request.message << QStringLiteral("--output-format")
              << QStringLiteral("stream-json") << QStringLiteral("--verbose");

    if (!request.mcpConfigPath.isEmpty()) {
      arguments << QStringLiteral("--mcp-config") << request.mcpConfigPath;
      // Non-interactive `-p` mode has no human to answer a permission
      // prompt, so every tool call is denied by default. Pre-approve only
      // our own MCP tools rather than `--dangerously-skip-permissions`,
      // which would also unlock the CLI's built-in Bash/Write/Edit/etc.
      arguments << QStringLiteral("--allowedTools") << allowedMcpToolNames().join(QLatin1Char(','));
    }

    if (!request.resumeSessionId.isEmpty())
      arguments << QStringLiteral("--resume") << request.resumeSessionId;

    return arguments;
  }

}
