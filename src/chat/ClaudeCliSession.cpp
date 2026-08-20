#include "chat/ClaudeCliSession.h"

#include <QJsonArray>
#include <QJsonValue>

#include "chat/StreamJsonLineParser.h"

namespace {
  // `tool_result` content in stream-json is either a plain string or an
  // array of content blocks (mirroring the Messages API); flatten either
  // shape down to the text the dock renders.
  QString toolResultText(const QJsonValue& content) {
    if (content.isString())
      return content.toString();

    if (content.isArray()) {
      QStringList parts;
      for (const auto& blockValue : content.toArray()) {
        const QJsonObject block = blockValue.toObject();
        if (block.value(QStringLiteral("type")).toString() == QStringLiteral("text"))
          parts << block.value(QStringLiteral("text")).toString();
      }
      return parts.join(QStringLiteral("\n"));
    }

    return QString();
  }
}

namespace chat {

  struct ClaudeCliSession::Private {
    QProcess* process = nullptr;
    StreamJsonLineParser parser;
    QString capturedSessionId;
    bool running = false;

    bool resultReceived = false;
    bool resultSuccess = false;
    QString resultMessage;

    QString processErrorString;
    QByteArray capturedStderr;
  };

  ClaudeCliSession::ClaudeCliSession(QObject* parent)
      : QObject(parent),
        p(std::make_unique<Private>()) {
  }

  ClaudeCliSession::~ClaudeCliSession() = default;

  void ClaudeCliSession::start(const ClaudeCliRequest& request) {
    if (isRunning())
      return;

    p->parser = StreamJsonLineParser();
    p->capturedSessionId.clear();
    p->resultReceived = false;
    p->resultSuccess = false;
    p->resultMessage.clear();
    p->processErrorString.clear();
    p->capturedStderr.clear();

    p->process = new QProcess(this);
    p->process->setProgram(request.executable);
    p->process->setArguments(claudeCliArguments(request));

    connect(p->process, &QProcess::readyReadStandardOutput, this,
            &ClaudeCliSession::handleReadyRead);
    connect(p->process, &QProcess::readyReadStandardError, this,
            &ClaudeCliSession::handleReadyReadStandardError);
    connect(p->process, &QProcess::finished, this, &ClaudeCliSession::handleProcessFinished);
    connect(p->process, &QProcess::errorOccurred, this, &ClaudeCliSession::handleErrorOccurred);

    p->running = true;
    p->process->start();
    // The one-shot-per-send model never writes an interactive stdin turn;
    // --input-format stream-json only matters for the -p message the CLI
    // already received on argv, so close stdin immediately rather than
    // leaving the child waiting on input that will never arrive.
    p->process->closeWriteChannel();
  }

  bool ClaudeCliSession::isRunning() const {
    return p->running;
  }

  void ClaudeCliSession::cancel() {
    if (p->process && p->running)
      p->process->kill();
  }

  QString ClaudeCliSession::sessionId() const {
    return p->capturedSessionId;
  }

  void ClaudeCliSession::handleReadyRead() {
    if (!p->process)
      return;

    const QByteArray chunk = p->process->readAllStandardOutput();
    for (const auto& event : p->parser.feed(chunk))
      dispatchStreamEvent(event);
  }

  void ClaudeCliSession::handleReadyReadStandardError() {
    if (!p->process)
      return;

    p->capturedStderr += p->process->readAllStandardError();
  }

  void ClaudeCliSession::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    handleReadyRead();
    handleReadyReadStandardError();
    for (const auto& event : p->parser.flush())
      dispatchStreamEvent(event);

    p->running = false;

    bool success = false;
    QString errorMessage;
    if (p->resultReceived) {
      success = p->resultSuccess;
      if (!success)
        errorMessage = p->resultMessage;
    } else {
      success = (exitStatus == QProcess::NormalExit && exitCode == 0);
      if (!success) {
        // Prefer the CLI's own stderr — "claude exited with code 1" tells
        // an operator nothing; the auth/config error claude printed does.
        const QString stderrText = QString::fromUtf8(p->capturedStderr).trimmed();
        if (!stderrText.isEmpty())
          errorMessage = stderrText;
        else if (!p->processErrorString.isEmpty())
          errorMessage = p->processErrorString;
        else
          errorMessage = tr("claude exited with code %1").arg(exitCode);
      }
    }

    emit finished(success, errorMessage);
  }

  void ClaudeCliSession::handleErrorOccurred(QProcess::ProcessError error) {
    p->processErrorString = p->process ? p->process->errorString() : QString();

    // FailedToStart is the one QProcess error that never gets a matching
    // finished() signal (the child never actually launched), so it has to
    // be reported here instead of from handleProcessFinished().
    if (error == QProcess::FailedToStart) {
      p->running = false;
      emit finished(false, p->processErrorString);
    }
  }

  void ClaudeCliSession::dispatchStreamEvent(const QJsonObject& event) {
    if (p->capturedSessionId.isEmpty()) {
      const QString sessionId = event.value(QStringLiteral("session_id")).toString();
      if (!sessionId.isEmpty()) {
        p->capturedSessionId = sessionId;
        emit sessionIdCaptured(sessionId);
      }
    }

    const QString type = event.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("assistant"))
      handleAssistantMessage(event);
    else if (type == QStringLiteral("user"))
      handleUserMessage(event);
    else if (type == QStringLiteral("result"))
      handleResultMessage(event);
  }

  void ClaudeCliSession::handleAssistantMessage(const QJsonObject& event) {
    const QJsonArray content =
      event.value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toArray();

    for (const auto& blockValue : content) {
      const QJsonObject block = blockValue.toObject();
      const QString blockType = block.value(QStringLiteral("type")).toString();

      if (blockType == QStringLiteral("text")) {
        const QString text = block.value(QStringLiteral("text")).toString();
        if (!text.isEmpty())
          emit assistantText(text);
      } else if (blockType == QStringLiteral("tool_use")) {
        emit toolCallStarted(block.value(QStringLiteral("id")).toString(),
                             block.value(QStringLiteral("name")).toString(),
                             block.value(QStringLiteral("input")).toObject());
      }
    }
  }

  void ClaudeCliSession::handleUserMessage(const QJsonObject& event) {
    const QJsonArray content =
      event.value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toArray();

    for (const auto& blockValue : content) {
      const QJsonObject block = blockValue.toObject();
      if (block.value(QStringLiteral("type")).toString() != QStringLiteral("tool_result"))
        continue;

      emit toolCallFinished(block.value(QStringLiteral("tool_use_id")).toString(),
                            toolResultText(block.value(QStringLiteral("content"))),
                            block.value(QStringLiteral("is_error")).toBool(false));
    }
  }

  void ClaudeCliSession::handleResultMessage(const QJsonObject& event) {
    p->resultReceived = true;
    p->resultSuccess = !event.value(QStringLiteral("is_error")).toBool(false);
    p->resultMessage = event.value(QStringLiteral("result")).toString();
  }

}
