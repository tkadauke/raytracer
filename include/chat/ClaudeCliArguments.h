#pragma once

#include <QString>
#include <QStringList>

namespace chat {

  /**
    * One turn's worth of parameters for spawning the `claude` CLI in
    * non-interactive stream-json mode.
    */
  struct ClaudeCliRequest {
    /// The user's message text, passed via `-p`.
    QString message;
    /// Path to the `--mcp-config` JSON file written by
    /// `mcp::writeMcpConfig()`. Left empty to omit `--mcp-config` entirely
    /// (e.g. when the embedded MCP server failed to start) rather than pass
    /// a path the CLI can't open.
    QString mcpConfigPath;
    /// Session id to continue via `--resume`. Empty starts a fresh session
    /// with no `--resume` flag — see `ChatThread`'s "first send has no
    /// session id yet" case.
    QString resumeSessionId;
    /// Overridable so tests can point at a fixture stand-in instead of a
    /// real, already-authenticated `claude` install.
    QString executable = QStringLiteral("claude");
  };

  /**
    * Builds the exact argv `ClaudeCliSession::start()` hands to `QProcess`:
    * `-p "<message>" --output-format stream-json --input-format stream-json
    * [--mcp-config <path>] [--resume <session_id>]`. Pure and
    * process-free, so the framing this issue asks to be testable without a
    * live CLI can be pinned directly.
    */
  QStringList claudeCliArguments(const ClaudeCliRequest& request);

}
