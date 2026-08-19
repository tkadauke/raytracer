#include "chat/StreamJsonLineParser.h"

#include <QJsonDocument>

namespace chat {

  QVector<QJsonObject> StreamJsonLineParser::feed(const QByteArray& chunk) {
    m_buffer += chunk;
    return consumeCompleteLines();
  }

  QVector<QJsonObject> StreamJsonLineParser::flush() {
    QVector<QJsonObject> result;
    const QByteArray line = m_buffer.trimmed();
    m_buffer.clear();
    if (line.isEmpty())
      return result;

    const QJsonDocument doc = QJsonDocument::fromJson(line);
    if (doc.isObject())
      result.push_back(doc.object());
    return result;
  }

  QVector<QJsonObject> StreamJsonLineParser::consumeCompleteLines() {
    QVector<QJsonObject> result;

    int newlineIndex = -1;
    while ((newlineIndex = m_buffer.indexOf('\n')) != -1) {
      const QByteArray line = m_buffer.left(newlineIndex).trimmed();
      m_buffer.remove(0, newlineIndex + 1);

      if (line.isEmpty())
        continue;

      const QJsonDocument doc = QJsonDocument::fromJson(line);
      if (doc.isObject())
        result.push_back(doc.object());
    }

    return result;
  }

}
