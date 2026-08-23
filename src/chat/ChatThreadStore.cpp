#include "chat/ChatThreadStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <algorithm>

namespace chat {

  namespace {
    QString roleToString(ChatMessageRole role) {
      switch (role) {
      case ChatMessageRole::User:
        return QStringLiteral("user");
      case ChatMessageRole::Assistant:
        return QStringLiteral("assistant");
      case ChatMessageRole::ToolCall:
        return QStringLiteral("tool_call");
      case ChatMessageRole::ToolResult:
        return QStringLiteral("tool_result");
      case ChatMessageRole::Error:
        return QStringLiteral("error");
      }
      return QStringLiteral("error");
    }

    ChatMessageRole roleFromString(const QString& value) {
      if (value == QStringLiteral("user"))
        return ChatMessageRole::User;
      if (value == QStringLiteral("assistant"))
        return ChatMessageRole::Assistant;
      if (value == QStringLiteral("tool_call"))
        return ChatMessageRole::ToolCall;
      if (value == QStringLiteral("tool_result"))
        return ChatMessageRole::ToolResult;
      return ChatMessageRole::Error;
    }

    QJsonObject messageToJson(const ChatMessage& message) {
      QJsonObject json;
      json[QStringLiteral("role")] = roleToString(message.role);
      if (!message.text.isEmpty())
        json[QStringLiteral("text")] = message.text;
      if (!message.toolName.isEmpty())
        json[QStringLiteral("toolName")] = message.toolName;
      if (!message.toolUseId.isEmpty())
        json[QStringLiteral("toolUseId")] = message.toolUseId;
      if (!message.toolInput.isEmpty())
        json[QStringLiteral("toolInput")] = message.toolInput;
      if (message.toolIsError)
        json[QStringLiteral("toolIsError")] = true;
      return json;
    }

    ChatMessage messageFromJson(const QJsonObject& json) {
      ChatMessage message;
      message.role = roleFromString(json.value(QStringLiteral("role")).toString());
      message.text = json.value(QStringLiteral("text")).toString();
      message.toolName = json.value(QStringLiteral("toolName")).toString();
      message.toolUseId = json.value(QStringLiteral("toolUseId")).toString();
      message.toolInput = json.value(QStringLiteral("toolInput")).toObject();
      message.toolIsError = json.value(QStringLiteral("toolIsError")).toBool(false);
      return message;
    }

    QJsonObject recordToJson(const ChatThreadRecord& record) {
      QJsonObject json;
      json[QStringLiteral("id")] = record.id;
      json[QStringLiteral("name")] = record.name;
      json[QStringLiteral("sessionId")] = record.sessionId;

      QJsonArray messages;
      for (const auto& message : record.messages)
        messages.append(messageToJson(message));
      json[QStringLiteral("messages")] = messages;

      return json;
    }

    // Returns std::nullopt-like empty-id record on malformed input; callers
    // check record.id.isEmpty() to detect and skip a bad file rather than
    // letting a corrupt thread file crash the whole load.
    ChatThreadRecord recordFromJson(const QJsonObject& json) {
      ChatThreadRecord record;
      record.id = json.value(QStringLiteral("id")).toString();
      record.name = json.value(QStringLiteral("name")).toString();
      record.sessionId = json.value(QStringLiteral("sessionId")).toString();

      for (const auto& value : json.value(QStringLiteral("messages")).toArray())
        record.messages.push_back(messageFromJson(value.toObject()));

      return record;
    }
  }

  ChatThreadStore::ChatThreadStore(QString baseDirectory)
      : m_baseDirectory(std::move(baseDirectory)) {
  }

  QString ChatThreadStore::defaultBaseDirectory() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
      QStringLiteral("/chats");
  }

  QString ChatThreadStore::sceneDirectory(const QString& sceneId) const {
    return m_baseDirectory + QLatin1Char('/') + sceneId;
  }

  QString ChatThreadStore::threadFilePath(const QString& sceneId, const QString& threadId) const {
    return sceneDirectory(sceneId) + QLatin1Char('/') + threadId + QStringLiteral(".json");
  }

  std::vector<ChatThreadRecord> ChatThreadStore::loadThreads(const QString& sceneId) const {
    std::vector<ChatThreadRecord> records;

    const QDir dir(sceneDirectory(sceneId));
    if (!dir.exists())
      return records;

    const QFileInfoList files = dir.entryInfoList(QStringList{QStringLiteral("*.json")},
                                                   QDir::Files, QDir::Name);
    for (const QFileInfo& fileInfo : files) {
      QFile file(fileInfo.absoluteFilePath());
      if (!file.open(QIODevice::ReadOnly))
        continue;

      QJsonParseError error;
      const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
      if (error.error != QJsonParseError::NoError || !doc.isObject())
        continue;

      ChatThreadRecord record = recordFromJson(doc.object());
      if (record.id.isEmpty())
        continue;

      records.push_back(std::move(record));
    }

    std::sort(records.begin(), records.end(),
              [](const ChatThreadRecord& a, const ChatThreadRecord& b) { return a.name < b.name; });

    return records;
  }

  bool ChatThreadStore::saveThread(const QString& sceneId, const ChatThreadRecord& record) const {
    if (!QDir().mkpath(sceneDirectory(sceneId)))
      return false;

    QFile file(threadFilePath(sceneId, record.id));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
      return false;

    file.write(QJsonDocument(recordToJson(record)).toJson(QJsonDocument::Indented));
    return true;
  }

  bool ChatThreadStore::deleteThread(const QString& sceneId, const QString& threadId) const {
    QFile file(threadFilePath(sceneId, threadId));
    return !file.exists() || file.remove();
  }

}
