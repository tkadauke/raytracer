#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>

#include "chat/ChatMessage.h"
#include "chat/ChatThread.h"
#include "chat/ChatThreadStore.h"
#include "widgets/chat/ChatDockWidget.h"

#include "test/helpers/GuiTestHelper.h"

namespace ChatDockWidgetTest {

  namespace {
    const QString kFixtureExecutable = QStringLiteral("test/fixtures/claude/fake_claude.py");

    void processPendingDeletes() {
      QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
      QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
  }

  class ChatDockWidgetTest : public ::testing::GuiTest {};

  TEST_F(ChatDockWidgetTest, StartsWithOneThreadAlreadyOpen) {
    ChatDockWidget dock;

    EXPECT_EQ(1, dock.threadCount());
    ASSERT_NE(nullptr, dock.threadAt(0));
    EXPECT_EQ(QStringLiteral("Chat 1"), dock.threadAt(0)->name());
  }

  TEST_F(ChatDockWidgetTest, AddThreadOpensANewNumberedTab) {
    ChatDockWidget dock;
    chat::ChatThread* second = dock.addThread();

    EXPECT_EQ(2, dock.threadCount());
    ASSERT_NE(nullptr, second);
    EXPECT_EQ(QStringLiteral("Chat 2"), second->name());
  }

  TEST_F(ChatDockWidgetTest, AddThreadHonorsAnExplicitName) {
    ChatDockWidget dock;
    chat::ChatThread* named = dock.addThread(QStringLiteral("Scene Cleanup"));

    EXPECT_EQ(QStringLiteral("Scene Cleanup"), named->name());
  }

  TEST_F(ChatDockWidgetTest, ClosingATabRemovesItsThread) {
    ChatDockWidget dock;
    dock.addThread();
    ASSERT_EQ(2, dock.threadCount());

    auto* tabs = dock.findChild<QTabWidget*>(QStringLiteral("chatThreadTabs"));
    ASSERT_NE(nullptr, tabs);

    // Exercises the same signal the tab bar's close button emits.
    QMetaObject::invokeMethod(tabs, "tabCloseRequested", Q_ARG(int, 0));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    EXPECT_EQ(1, dock.threadCount());
  }

  TEST_F(ChatDockWidgetTest, TypingAndSendingDrivesTheUnderlyingThread) {
    ChatDockWidget dock;
    dock.setClaudeExecutable(kFixtureExecutable);

    auto* input = dock.findChild<QLineEdit*>(QStringLiteral("chatInput"));
    auto* sendButton = dock.findChild<QPushButton*>(QStringLiteral("chatSendButton"));
    ASSERT_NE(nullptr, input);
    ASSERT_NE(nullptr, sendButton);

    input->setText(QStringLiteral("add a red sphere"));
    sendButton->click();

    chat::ChatThread* thread = dock.threadAt(0);
    ASSERT_NE(nullptr, thread);
    while (thread->isBusy())
      QCoreApplication::processEvents(QEventLoop::AllEvents, 20);

    ASSERT_GE(thread->messages().size(), 2u);
    EXPECT_EQ(chat::ChatMessageRole::User, thread->messages()[0].role);
    EXPECT_TRUE(input->text().isEmpty());
  }

  // --- Persistence (roadmap §4.6.i chat persistence) ---------------------

  TEST_F(ChatDockWidgetTest, UnsavedSceneThreadsAreDraftOnlyAndNeverReachTheStore) {
    QTemporaryDir storeDir;
    ASSERT_TRUE(storeDir.isValid());

    ChatDockWidget dock;
    dock.setThreadStoreBaseDirectory(storeDir.path());
    dock.setScene(QStringLiteral("scene-unsaved"), /*sceneIsPersisted=*/false);
    dock.renameThread(0, QStringLiteral("Draft Thread"));
    dock.addThread(QStringLiteral("Another Draft"));

    chat::ChatThreadStore store(storeDir.path());
    EXPECT_TRUE(store.loadThreads(QStringLiteral("scene-unsaved")).empty());
  }

  TEST_F(ChatDockWidgetTest, NewThreadOnAPersistedSceneIsWrittenToTheStoreImmediately) {
    QTemporaryDir storeDir;
    ASSERT_TRUE(storeDir.isValid());

    ChatDockWidget dock;
    dock.setThreadStoreBaseDirectory(storeDir.path());
    dock.setScene(QStringLiteral("scene-1"), /*sceneIsPersisted=*/true);

    chat::ChatThreadStore store(storeDir.path());
    // setScene() opens one default thread when nothing was persisted yet.
    ASSERT_EQ(1u, store.loadThreads(QStringLiteral("scene-1")).size());
  }

  TEST_F(ChatDockWidgetTest, RenamingAThreadPersistsTheNewName) {
    QTemporaryDir storeDir;
    ASSERT_TRUE(storeDir.isValid());

    ChatDockWidget dock;
    dock.setThreadStoreBaseDirectory(storeDir.path());
    dock.setScene(QStringLiteral("scene-1"), /*sceneIsPersisted=*/true);
    dock.renameThread(0, QStringLiteral("Renamed Thread"));

    chat::ChatThreadStore store(storeDir.path());
    const auto loaded = store.loadThreads(QStringLiteral("scene-1"));
    ASSERT_EQ(1u, loaded.size());
    EXPECT_EQ(QStringLiteral("Renamed Thread"), loaded[0].name);
  }

  TEST_F(ChatDockWidgetTest, DeletingAThreadRemovesItsOnDiskFile) {
    QTemporaryDir storeDir;
    ASSERT_TRUE(storeDir.isValid());

    ChatDockWidget dock;
    dock.setThreadStoreBaseDirectory(storeDir.path());
    dock.setScene(QStringLiteral("scene-1"), /*sceneIsPersisted=*/true);
    dock.addThread(QStringLiteral("Second Thread"));
    ASSERT_EQ(2, dock.threadCount());

    auto* tabs = dock.findChild<QTabWidget*>(QStringLiteral("chatThreadTabs"));
    ASSERT_NE(nullptr, tabs);
    QMetaObject::invokeMethod(tabs, "tabCloseRequested", Q_ARG(int, 1));
    processPendingDeletes();

    EXPECT_EQ(1, dock.threadCount());

    chat::ChatThreadStore store(storeDir.path());
    const auto loaded = store.loadThreads(QStringLiteral("scene-1"));
    ASSERT_EQ(1u, loaded.size());
    EXPECT_NE(QStringLiteral("Second Thread"), loaded[0].name);
  }

  TEST_F(ChatDockWidgetTest, SwitchingSceneShowsOnlyThatScenesThreads) {
    QTemporaryDir storeDir;
    ASSERT_TRUE(storeDir.isValid());

    ChatDockWidget dock;
    dock.setThreadStoreBaseDirectory(storeDir.path());

    dock.setScene(QStringLiteral("scene-a"), /*sceneIsPersisted=*/true);
    dock.renameThread(0, QStringLiteral("Scene A Thread"));

    dock.setScene(QStringLiteral("scene-b"), /*sceneIsPersisted=*/true);
    processPendingDeletes();
    ASSERT_EQ(1, dock.threadCount());
    EXPECT_NE(QStringLiteral("Scene A Thread"), dock.threadAt(0)->name());

    dock.setScene(QStringLiteral("scene-a"), /*sceneIsPersisted=*/true);
    processPendingDeletes();
    ASSERT_EQ(1, dock.threadCount());
    EXPECT_EQ(QStringLiteral("Scene A Thread"), dock.threadAt(0)->name());
  }

  TEST_F(ChatDockWidgetTest, SaveAsPromotesDraftThreadsToTheStoreInsteadOfDiscardingThem) {
    QTemporaryDir storeDir;
    ASSERT_TRUE(storeDir.isValid());

    ChatDockWidget dock;
    dock.setThreadStoreBaseDirectory(storeDir.path());
    dock.setScene(QStringLiteral("scene-1"), /*sceneIsPersisted=*/false);
    dock.renameThread(0, QStringLiteral("Was A Draft"));

    chat::ChatThreadStore store(storeDir.path());
    ASSERT_TRUE(store.loadThreads(QStringLiteral("scene-1")).empty());

    // Same scene id, now persisted — as MainWindow::saveFileAs() does after
    // a first save assigns the (already-existing) scene a file on disk.
    dock.setScene(QStringLiteral("scene-1"), /*sceneIsPersisted=*/true);

    const auto loaded = store.loadThreads(QStringLiteral("scene-1"));
    ASSERT_EQ(1u, loaded.size());
    EXPECT_EQ(QStringLiteral("Was A Draft"), loaded[0].name);
  }

  TEST_F(ChatDockWidgetTest, AppRestartRoundTripsSessionIdAndMessageHistory) {
    QTemporaryDir storeDir;
    ASSERT_TRUE(storeDir.isValid());

    QString capturedSessionId;
    {
      ChatDockWidget dock;
      dock.setThreadStoreBaseDirectory(storeDir.path());
      dock.setClaudeExecutable(kFixtureExecutable);
      dock.setScene(QStringLiteral("scene-1"), /*sceneIsPersisted=*/true);
      dock.renameThread(0, QStringLiteral("Roundtrip Thread"));

      chat::ChatThread* thread = dock.threadAt(0);
      ASSERT_NE(nullptr, thread);
      thread->sendMessage(QStringLiteral("add a red sphere"), QString(), kFixtureExecutable);
      while (thread->isBusy())
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);

      capturedSessionId = thread->sessionId();
      ASSERT_FALSE(capturedSessionId.isEmpty());
    }

    // A brand-new dock stands in for the app restarting: nothing but the
    // on-disk store carries state across.
    ChatDockWidget restarted;
    restarted.setThreadStoreBaseDirectory(storeDir.path());
    restarted.setScene(QStringLiteral("scene-1"), /*sceneIsPersisted=*/true);

    ASSERT_EQ(1, restarted.threadCount());
    chat::ChatThread* restoredThread = restarted.threadAt(0);
    ASSERT_NE(nullptr, restoredThread);
    EXPECT_EQ(QStringLiteral("Roundtrip Thread"), restoredThread->name());
    EXPECT_EQ(capturedSessionId, restoredThread->sessionId());

    const auto& messages = restoredThread->messages();
    ASSERT_GE(messages.size(), 4u);
    EXPECT_EQ(chat::ChatMessageRole::User, messages[0].role);
    EXPECT_EQ(chat::ChatMessageRole::Assistant, messages[1].role);
    EXPECT_EQ(chat::ChatMessageRole::ToolCall, messages[2].role);
    EXPECT_EQ(chat::ChatMessageRole::ToolResult, messages[3].role);
  }

}
