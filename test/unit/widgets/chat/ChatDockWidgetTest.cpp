#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>

#include "chat/ChatMessage.h"
#include "chat/ChatThread.h"
#include "widgets/chat/ChatDockWidget.h"

#include "test/helpers/GuiTestHelper.h"

namespace ChatDockWidgetTest {

  namespace {
    const QString kFixtureExecutable = QStringLiteral("test/fixtures/claude/fake_claude.py");
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

}
