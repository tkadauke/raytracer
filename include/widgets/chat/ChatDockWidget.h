#pragma once

#include <QString>
#include <QWidget>
#include <memory>

namespace chat {
  class ChatThread;
}

/**
  * Top-level widget for the AI-native chat side panel (roadmap §4.6.i),
  * placed in a QDockWidget by MainWindow following the existing
  * PropertyEditorWidget/PreviewDisplayWidget/RenderGraphInspectorWidget
  * dock pattern. Manages multiple chat::ChatThread instances as tabs, each
  * rendered by a ChatThreadPanel — "multiple named threads against the
  * currently open scene document" per the AI-native-modeler epic.
  *
  * Starts with one thread already open so there's somewhere to type
  * immediately; a corner "+" button on the tab bar opens more.
  */
class ChatDockWidget : public QWidget {
  Q_OBJECT

public:
  explicit ChatDockWidget(QWidget* parent = nullptr);
  ~ChatDockWidget() override;

  /**
    * The `--mcp-config` path every thread's sends use (from
    * mcp::writeMcpConfig()). MainWindow only knows this after the embedded
    * MCP server has started, which happens after dock construction, so
    * this is a setter rather than a constructor argument — it applies to
    * already-open threads too, not just ones opened afterward.
    */
  void setMcpConfigPath(const QString& path);

  /// Overridable so tests can point every thread's sends at a fixture
  /// stand-in instead of a real `claude` install.
  void setClaudeExecutable(const QString& executable);

  /// Creates a new thread/tab. @p name defaults to "Chat N". Returns the
  /// new thread so callers (tests, mainly) can drive it directly.
  chat::ChatThread* addThread(const QString& name = QString());

  [[nodiscard]] int threadCount() const;
  [[nodiscard]] chat::ChatThread* threadAt(int index) const;

private slots:
  void newThreadButtonClicked();
  void closeTabRequested(int index);

private:
  struct Private;
  std::unique_ptr<Private> p;
};
