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
  * immediately; a corner "+" button on the tab bar opens more. The tab bar
  * doubles as the thread list the persistence issue asks for: tab labels
  * list the current scene's threads, the "+" corner button creates one,
  * double-clicking a label renames it, and each tab's close button deletes
  * it (including its on-disk file, once the scene is persisted — see
  * setScene()). Threads persist under chat::ChatThreadStore, keyed by the
  * open scene's stable id; setScene() switches which scene's threads are
  * visible and reloads them from disk.
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

  /// Overridable so tests can redirect thread persistence at a
  /// QTemporaryDir instead of the real QStandardPaths::AppDataLocation.
  /// Applies to the next setScene() call, not retroactively.
  void setThreadStoreBaseDirectory(const QString& baseDirectory);

  /**
    * Switches the dock to @p sceneId's threads (roadmap §4.6.i chat
    * persistence): tabs are torn down and rebuilt from whatever
    * ChatThreadStore has on disk for @p sceneId, or a single fresh draft
    * thread if there's nothing persisted yet.
    *
    * @p sceneIsPersisted mirrors "has this scene been saved to a file at
    * least once" (MainWindow's `fileName` being non-null) — a scene that
    * has never been saved has no stable on-disk identity worth writing
    * chats under, so threads against it stay draft-only (in-memory, never
    * reach ChatThreadStore) until the scene is first saved.
    *
    * Calling this again with the *same* sceneId (e.g. "Save As" promoting
    * a previously-unsaved scene to persisted) keeps the currently open
    * threads instead of reloading — only sceneIsPersisted's transition
    * matters, and false -> true flushes the open threads to disk instead
    * of discarding them.
    */
  void setScene(const QString& sceneId, bool sceneIsPersisted);

  /// Creates a new thread/tab. @p name defaults to "Chat N". Returns the
  /// new thread so callers (tests, mainly) can drive it directly.
  chat::ChatThread* addThread(const QString& name = QString());

  /// Renames the thread at @p index and persists the rename immediately
  /// (if the current scene is persisted). Public (rather than only reachable
  /// via the tab bar's double-click prompt) so tests can drive it directly.
  void renameThread(int index, const QString& name);

  [[nodiscard]] int threadCount() const;
  [[nodiscard]] chat::ChatThread* threadAt(int index) const;

private slots:
  void newThreadButtonClicked();
  void closeTabRequested(int index);
  void tabDoubleClicked(int index);

private:
  /// Wraps @p thread in a ChatThreadPanel, wires the mcp/executable config
  /// and the persist-on-change connections, and adds it as a tab. Does not
  /// change which tab is current — callers that want the new tab focused
  /// (addThread()) do that themselves.
  void installThreadTab(chat::ChatThread* thread);

  /// Writes @p thread's current state to the store, if the current scene
  /// is persisted. No-op (not just a failed write) while a draft scene is
  /// open — see setScene().
  void persistThread(chat::ChatThread* thread);

  struct Private;
  std::unique_ptr<Private> p;
};
