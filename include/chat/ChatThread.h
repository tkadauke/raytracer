#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include <vector>

#include "chat/ChatMessage.h"

namespace chat {

  /**
    * One chat thread = one `claude` CLI session, per the chat-dock issue's
    * session model. Holds the in-memory transcript (persistence across app
    * restarts is explicitly out of scope for this Job — see the issue) and
    * drives a fresh `ClaudeCliSession` per `sendMessage()` call: the first
    * send has no session id yet and omits `--resume`; every call after that
    * resumes the session id captured from the first one.
    *
    * "One live `claude` subprocess per active/in-flight thread; idle
    * threads have no running process" — `ChatThread` owns its current
    * `ClaudeCliSession` only while a turn is in flight (isBusy()) and
    * drops it (deleteLater()) once finished() fires, rather than keeping
    * one alive per thread for the thread's whole lifetime.
    */
  class ChatThread : public QObject {
    Q_OBJECT

  public:
    /// Generates a fresh id (QUuid, matching Element's own id scheme) — for
    /// brand-new threads.
    explicit ChatThread(QString name, QObject* parent = nullptr);

    /// Explicit id — for restoring a thread ChatThreadStore already
    /// assigned an id to on a previous run. Callers should follow up with
    /// restore() to bring back sessionId()/messages() too.
    ChatThread(QString id, QString name, QObject* parent = nullptr);

    ~ChatThread() override;

    /// Stable identity used as the on-disk filename by ChatThreadStore;
    /// never changes for the lifetime of the thread.
    [[nodiscard]] const QString& id() const;

    [[nodiscard]] const QString& name() const;
    void setName(QString name);

    /// Empty until the first sendMessage() turn captures one.
    [[nodiscard]] const QString& sessionId() const;

    /// True while a `claude` subprocess for this thread is in flight.
    [[nodiscard]] bool isBusy() const;

    [[nodiscard]] const std::vector<ChatMessage>& messages() const;

    /**
      * Appends a User message and spawns a `claude` turn for it. No-op
      * while isBusy() is true (the UI should disable sending) or if
      * @p text is blank. @p executable is overridable so tests can point
      * at a fixture stand-in instead of a real `claude` install.
      */
    void sendMessage(const QString& text, const QString& mcpConfigPath,
                     const QString& executable = QStringLiteral("claude"));

    /**
      * Restores persisted state loaded from a ChatThreadStore record: the
      * captured `claude` session id (so the next sendMessage() resumes it)
      * and the rendered transcript. Does not emit messageAppended for the
      * restored messages — callers that need to render history (e.g.
      * ChatThreadPanel) should iterate messages() directly after calling
      * this, rather than relying on the per-append signal.
      */
    void restore(QString sessionId, std::vector<ChatMessage> messages);

  signals:
    /// Fired after any message (user, assistant, tool call/result, error)
    /// is appended; @p index is its position in messages().
    void messageAppended(int index);
    void busyChanged(bool busy);

  private:
    void appendMessage(ChatMessage message);
    void setBusy(bool busy);

    void handleSessionIdCaptured(const QString& sessionId);
    void handleAssistantText(const QString& text);
    void handleToolCallStarted(const QString& toolUseId, const QString& toolName,
                               const QJsonObject& input);
    void handleToolCallFinished(const QString& toolUseId, const QString& resultText, bool isError);
    void handleFinished(bool success, const QString& errorMessage);

    struct Private;
    std::unique_ptr<Private> p;
  };

}
