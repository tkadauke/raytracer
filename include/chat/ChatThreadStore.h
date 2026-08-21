#pragma once

#include <QString>
#include <vector>

#include "chat/ChatMessage.h"

namespace chat {

  /// Full on-disk state of one persisted thread: everything ChatThread
  /// needs to restore itself after an app restart.
  struct ChatThreadRecord {
    QString id;
    QString name;
    QString sessionId;
    std::vector<ChatMessage> messages;
  };

  /**
    * Persists chat threads to `<baseDirectory>/<sceneId>/<threadId>.json`,
    * keyed by the open scene's stable `Element::id()` per the chat-persistence
    * issue. `baseDirectory` defaults to `QStandardPaths::AppDataLocation +
    * "/chats"` but is a constructor argument (not hardcoded) so tests can
    * point it at a `QTemporaryDir` instead of the real user data directory.
    *
    * Scenes that have never been saved have no stable on-disk identity to
    * key a directory by — callers should not call save/deleteThread for
    * threads against such a scene at all (see ChatDockWidget::setScene's
    * "draft-only" handling), rather than relying on this class to guard
    * against it.
    */
  class ChatThreadStore {
  public:
    explicit ChatThreadStore(QString baseDirectory = defaultBaseDirectory());

    [[nodiscard]] static QString defaultBaseDirectory();

    /// Returns every persisted thread for @p sceneId, sorted by name for
    /// deterministic ordering. Malformed thread files are skipped rather
    /// than failing the whole load.
    [[nodiscard]] std::vector<ChatThreadRecord> loadThreads(const QString& sceneId) const;

    /// Writes (or overwrites) @p record's file under @p sceneId. Creates
    /// the scene's directory if it doesn't exist yet.
    bool saveThread(const QString& sceneId, const ChatThreadRecord& record) const;

    /// Removes @p threadId's file under @p sceneId. Returns true if the
    /// file was removed or already absent.
    bool deleteThread(const QString& sceneId, const QString& threadId) const;

  private:
    [[nodiscard]] QString sceneDirectory(const QString& sceneId) const;
    [[nodiscard]] QString threadFilePath(const QString& sceneId, const QString& threadId) const;

    QString m_baseDirectory;
  };

}
