#pragma once

#include <QString>

namespace mcp {

  class McpServer;

  /**
    * Writes the JSON config file that `claude --mcp-config <path>` consumes
    * to reach the embedded MCP server: the HTTP/SSE transport type, the
    * loopback SSE URL, and the per-session bearer token as an Authorization
    * header.
    *
    * The file lives under `QStandardPaths::AppDataLocation` and is
    * (re)written every time the server starts, so it always names the
    * current port and token. Its permissions are restricted to the owner
    * since it carries a live bearer token.
    *
    * @returns the absolute path the config was written to, or an empty
    *   string if @p server is not running or the file could not be written.
    */
  QString writeMcpConfig(const McpServer& server);

}
