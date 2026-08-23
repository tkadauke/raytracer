#include "chat/ChatThread.h"

#include <QMap>
#include <QUuid>

#include "chat/ClaudeCliSession.h"

namespace chat {

  struct ChatThread::Private {
    QString id;
    QString name;
    QString sessionId;
    bool busy = false;
    std::vector<ChatMessage> messages;
    ClaudeCliSession* session = nullptr;
    // toolUseId -> toolName, so the ToolResult entry can carry the same
    // name the matching ToolCall entry showed; ClaudeCliSession's
    // toolCallFinished signal only carries the id.
    QMap<QString, QString> pendingToolNames;
  };

  ChatThread::ChatThread(QString name, QObject* parent)
      : ChatThread(QUuid::createUuid().toString(), std::move(name), parent) {
  }

  ChatThread::ChatThread(QString id, QString name, QObject* parent)
      : QObject(parent),
        p(std::make_unique<Private>()) {
    p->id = std::move(id);
    p->name = std::move(name);
  }

  ChatThread::~ChatThread() = default;

  const QString& ChatThread::id() const {
    return p->id;
  }

  const QString& ChatThread::name() const {
    return p->name;
  }

  void ChatThread::setName(QString name) {
    p->name = std::move(name);
  }

  const QString& ChatThread::sessionId() const {
    return p->sessionId;
  }

  bool ChatThread::isBusy() const {
    return p->busy;
  }

  const std::vector<ChatMessage>& ChatThread::messages() const {
    return p->messages;
  }

  void ChatThread::sendMessage(const QString& text, const QString& mcpConfigPath,
                               const QString& executable) {
    if (isBusy() || text.trimmed().isEmpty())
      return;

    ChatMessage userMessage;
    userMessage.role = ChatMessageRole::User;
    userMessage.text = text;
    appendMessage(std::move(userMessage));

    ClaudeCliRequest request;
    request.message = text;
    request.mcpConfigPath = mcpConfigPath;
    request.resumeSessionId = p->sessionId;
    request.executable = executable;

    auto* session = new ClaudeCliSession(this);
    p->session = session;

    connect(session, &ClaudeCliSession::sessionIdCaptured, this,
            &ChatThread::handleSessionIdCaptured);
    connect(session, &ClaudeCliSession::assistantText, this, &ChatThread::handleAssistantText);
    connect(session, &ClaudeCliSession::toolCallStarted, this, &ChatThread::handleToolCallStarted);
    connect(session, &ClaudeCliSession::toolCallFinished, this,
            &ChatThread::handleToolCallFinished);
    connect(session, &ClaudeCliSession::finished, this, &ChatThread::handleFinished);

    setBusy(true);
    session->start(request);
  }

  void ChatThread::restore(QString sessionId, std::vector<ChatMessage> messages) {
    p->sessionId = std::move(sessionId);
    p->messages = std::move(messages);
  }

  void ChatThread::appendMessage(ChatMessage message) {
    p->messages.push_back(std::move(message));
    emit messageAppended(static_cast<int>(p->messages.size()) - 1);
  }

  void ChatThread::setBusy(bool busy) {
    if (p->busy == busy)
      return;
    p->busy = busy;
    emit busyChanged(busy);
  }

  void ChatThread::handleSessionIdCaptured(const QString& sessionId) {
    p->sessionId = sessionId;
  }

  void ChatThread::handleAssistantText(const QString& text) {
    ChatMessage message;
    message.role = ChatMessageRole::Assistant;
    message.text = text;
    appendMessage(std::move(message));
  }

  void ChatThread::handleToolCallStarted(const QString& toolUseId, const QString& toolName,
                                         const QJsonObject& input) {
    p->pendingToolNames.insert(toolUseId, toolName);

    ChatMessage message;
    message.role = ChatMessageRole::ToolCall;
    message.toolUseId = toolUseId;
    message.toolName = toolName;
    message.toolInput = input;
    appendMessage(std::move(message));
  }

  void ChatThread::handleToolCallFinished(const QString& toolUseId, const QString& resultText,
                                          bool isError) {
    ChatMessage message;
    message.role = ChatMessageRole::ToolResult;
    message.toolUseId = toolUseId;
    message.toolName = p->pendingToolNames.take(toolUseId);
    message.text = resultText;
    message.toolIsError = isError;
    appendMessage(std::move(message));
  }

  void ChatThread::handleFinished(bool success, const QString& errorMessage) {
    if (!success) {
      ChatMessage message;
      message.role = ChatMessageRole::Error;
      message.text = errorMessage.isEmpty() ? tr("claude exited without a specific error message")
                                            : errorMessage;
      appendMessage(std::move(message));
    }

    if (p->session) {
      p->session->deleteLater();
      p->session = nullptr;
    }

    setBusy(false);
  }

}
