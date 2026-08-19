#pragma once

#include <QString>
#include <QWidget>
#include <memory>

namespace chat {
  class ChatThread;
}

/**
  * Transcript + input for a single chat::ChatThread: a read-only rich-text
  * history (rendered via chat::formatChatMessageHtml() as messages stream
  * in) plus a line edit and Send button that call ChatThread::sendMessage().
  *
  * Does not own the ChatThread it wraps — see ChatDockWidget for the
  * ownership wiring (the thread is reparented under its panel so the two
  * are destroyed together).
  */
class ChatThreadPanel : public QWidget {
  Q_OBJECT

public:
  explicit ChatThreadPanel(chat::ChatThread* thread, QWidget* parent = nullptr);
  ~ChatThreadPanel() override;

  [[nodiscard]] chat::ChatThread* thread() const;

  /// Forwarded into every ClaudeCliRequest this panel's sends build.
  void setMcpConfigPath(const QString& path);

  /// Overridable so tests can point sends at a fixture stand-in instead of
  /// a real `claude` install.
  void setClaudeExecutable(const QString& executable);

private slots:
  void sendClicked();
  void handleMessageAppended(int index);
  void handleBusyChanged(bool busy);

private:
  struct Private;
  std::unique_ptr<Private> p;
};
