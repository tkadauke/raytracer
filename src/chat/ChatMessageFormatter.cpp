#include "chat/ChatMessageFormatter.h"

#include <QJsonDocument>

#include "chat/ChatMessage.h"

namespace {
  QString escaped(const QString& text) {
    return text.toHtmlEscaped().replace(QStringLiteral("\n"), QStringLiteral("<br>"));
  }

  QString compactJson(const QJsonObject& object) {
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
  }
}

namespace chat {

  QString formatChatMessageHtml(const ChatMessage& message) {
    switch (message.role) {
    case ChatMessageRole::User:
      return QStringLiteral("<p><b>You:</b> %1</p>").arg(escaped(message.text));

    case ChatMessageRole::Assistant:
      return QStringLiteral("<p><b>Claude:</b> %1</p>").arg(escaped(message.text));

    case ChatMessageRole::ToolCall:
      return QStringLiteral("<p style=\"color:#666\"><i>&#8594; tool call</i> "
                            "<code>%1</code>(%2)</p>")
        .arg(escaped(message.toolName), escaped(compactJson(message.toolInput)));

    case ChatMessageRole::ToolResult:
      if (message.toolIsError) {
        return QStringLiteral("<p style=\"color:#a00\"><i>&#8592; tool error</i> "
                              "<code>%1</code>: %2</p>")
          .arg(escaped(message.toolName), escaped(message.text));
      }
      return QStringLiteral("<p style=\"color:#666\"><i>&#8592; tool result</i> "
                            "<code>%1</code>: %2</p>")
        .arg(escaped(message.toolName), escaped(message.text));

    case ChatMessageRole::Error:
      return QStringLiteral("<p style=\"color:#a00\"><b>Error:</b> %1</p>")
        .arg(escaped(message.text));
    }

    return QString();
  }

}
