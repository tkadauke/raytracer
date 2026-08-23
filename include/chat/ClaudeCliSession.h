#pragma once

#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QString>
#include <memory>

#include "chat/ClaudeCliArguments.h"

namespace chat {

  /**
    * Wraps a single one-shot `claude -p ... --output-format stream-json`
    * subprocess: spawns it via `QProcess`, line-buffers stdout through
    * `StreamJsonLineParser`, and turns the parsed stream-json events into
    * Qt signals a `ChatThread` (or a test) can consume without knowing
    * anything about the wire format.
    *
    * One `ClaudeCliSession` is exactly one CLI invocation — "one live
    * `claude` subprocess per active/in-flight thread" from the chat-dock
    * issue. `ChatThread` constructs a fresh instance per `sendMessage()`
    * call rather than reusing one across turns, since `-p` is inherently
    * one-shot: the process exits after emitting its `result` event.
    *
    * Understood stream-json event shapes (Claude CLI's non-interactive
    * `-p --output-format stream-json` framing):
    * - `{"type":"system","subtype":"init","session_id":"...",...}` — and in
    *   general, whichever event arrives first carries `session_id`; that's
    *   what sessionIdCaptured() reports, regardless of `type`.
    * - `{"type":"assistant","message":{"content":[{"type":"text","text":"..."}
    *   or {"type":"tool_use","id":"...","name":"...","input":{...}}]}}`
    * - `{"type":"user","message":{"content":[{"type":"tool_result",
    *   "tool_use_id":"...","content":...,"is_error":bool}]}}` — tool
    *   results the CLI itself received from the MCP server, echoed back so
    *   the dock can render them.
    * - `{"type":"result","subtype":"success"|"error_...","is_error":bool,
    *   "result":"..."}` — final per-turn summary.
    */
  class ClaudeCliSession : public QObject {
    Q_OBJECT

  public:
    explicit ClaudeCliSession(QObject* parent = nullptr);
    ~ClaudeCliSession() override;

    /**
      * Spawns the subprocess described by @p request. No-op (returns
      * immediately) if a previous start() is still running — callers
      * should check isRunning() first.
      */
    void start(const ClaudeCliRequest& request);

    [[nodiscard]] bool isRunning() const;

    /// Kills the in-flight subprocess, if any; finished() still fires.
    void cancel();

    /// The session id captured from the first stream event, or empty if
    /// none has arrived yet (or start() was never called).
    [[nodiscard]] QString sessionId() const;

  signals:
    /// Fired once, the first time any stream event carries a session_id.
    void sessionIdCaptured(const QString& sessionId);

    /// Fired for each `text` content block in an `assistant` message.
    void assistantText(const QString& text);

    /// Fired for each `tool_use` content block in an `assistant` message.
    void toolCallStarted(const QString& toolUseId, const QString& toolName,
                         const QJsonObject& input);

    /// Fired for each `tool_result` content block in a `user` message.
    void toolCallFinished(const QString& toolUseId, const QString& resultText, bool isError);

    /**
      * Fired exactly once, when the subprocess has fully exited (or failed
      * to start at all). @p errorMessage is empty on success.
      */
    void finished(bool success, const QString& errorMessage);

  private slots:
    void handleReadyRead();
    void handleReadyReadStandardError();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleErrorOccurred(QProcess::ProcessError error);

  private:
    void dispatchStreamEvent(const QJsonObject& event);
    void handleAssistantMessage(const QJsonObject& event);
    void handleUserMessage(const QJsonObject& event);
    void handleResultMessage(const QJsonObject& event);

    struct Private;
    std::unique_ptr<Private> p;
  };

}
