#pragma once

#include <QJsonObject>
#include <QString>

namespace chat {

  enum class ChatMessageRole {
    User,
    Assistant,
    ToolCall,
    ToolResult,
    Error,
  };

  /**
    * One rendered entry in a `ChatThread`'s transcript. `ToolCall`/
    * `ToolResult` entries mirror `ClaudeCliSession`'s own
    * `toolCallStarted`/`toolCallFinished` signals, so the dock can show
    * which MCP tools an agent turn invoked live, not just its
    * reasoning-facing text — the acceptance criterion from the chat-dock
    * issue.
    */
  struct ChatMessage {
    ChatMessageRole role = ChatMessageRole::User;
    QString text;

    /// Populated for ToolCall (name + input) and ToolResult (name + error
    /// flag) only; empty/default otherwise.
    QString toolName;
    QString toolUseId;
    QJsonObject toolInput;
    bool toolIsError = false;
  };

}
