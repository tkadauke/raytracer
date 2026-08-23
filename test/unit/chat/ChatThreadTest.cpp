#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QSignalSpy>
#include <QString>

#include "chat/ChatThread.h"

#include "test/helpers/GuiTestHelper.h"

namespace ChatThreadTest {

  namespace {
    const QString kFixtureExecutable = QStringLiteral("test/fixtures/claude/fake_claude.py");
  }

  class ChatThreadTest : public ::testing::GuiTest {};

  TEST_F(ChatThreadTest, NameDefaultsToTheConstructorArgumentAndIsSettable) {
    chat::ChatThread thread(QStringLiteral("Untitled"));
    EXPECT_EQ(QStringLiteral("Untitled"), thread.name());

    thread.setName(QStringLiteral("Renamed"));
    EXPECT_EQ(QStringLiteral("Renamed"), thread.name());
  }

  TEST_F(ChatThreadTest, SendMessageAppendsUserTurnThenStreamsTheAgentTurn) {
    chat::ChatThread thread(QStringLiteral("Thread 1"));
    QSignalSpy busySpy(&thread, &chat::ChatThread::busyChanged);
    QSignalSpy appendedSpy(&thread, &chat::ChatThread::messageAppended);

    EXPECT_FALSE(thread.isBusy());
    thread.sendMessage(QStringLiteral("add a red sphere"), QString(), kFixtureExecutable);
    EXPECT_TRUE(thread.isBusy());
    ASSERT_EQ(1u, thread.messages().size());
    EXPECT_EQ(chat::ChatMessageRole::User, thread.messages()[0].role);

    while (thread.isBusy())
      QCoreApplication::processEvents(QEventLoop::AllEvents, 20);

    EXPECT_FALSE(thread.isBusy());
    EXPECT_GE(busySpy.size(), 2);

    const auto& messages = thread.messages();
    ASSERT_GE(messages.size(), 4u);
    EXPECT_EQ(chat::ChatMessageRole::User, messages[0].role);
    EXPECT_EQ(chat::ChatMessageRole::Assistant, messages[1].role);
    EXPECT_EQ(QStringLiteral("Echo: add a red sphere"), messages[1].text);
    EXPECT_EQ(chat::ChatMessageRole::ToolCall, messages[2].role);
    EXPECT_EQ(QStringLiteral("query_scene"), messages[2].toolName);
    EXPECT_EQ(chat::ChatMessageRole::ToolResult, messages[3].role);
    EXPECT_EQ(QStringLiteral("query_scene"), messages[3].toolName);
    EXPECT_FALSE(messages[3].toolIsError);

    EXPECT_FALSE(thread.sessionId().isEmpty());
    EXPECT_EQ(static_cast<int>(messages.size()), appendedSpy.size());
  }

  TEST_F(ChatThreadTest, SecondSendResumesTheSessionCapturedByTheFirst) {
    chat::ChatThread thread(QStringLiteral("Thread 1"));

    thread.sendMessage(QStringLiteral("first"), QString(), kFixtureExecutable);
    while (thread.isBusy())
      QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    const QString firstSessionId = thread.sessionId();
    ASSERT_FALSE(firstSessionId.isEmpty());

    thread.sendMessage(QStringLiteral("second"), QString(), kFixtureExecutable);
    while (thread.isBusy())
      QCoreApplication::processEvents(QEventLoop::AllEvents, 20);

    EXPECT_EQ(firstSessionId, thread.sessionId());
  }

  TEST_F(ChatThreadTest, SendMessageWhileBusyIsANoOp) {
    chat::ChatThread thread(QStringLiteral("Thread 1"));

    thread.sendMessage(QStringLiteral("first"), QString(), kFixtureExecutable);
    const size_t countAfterFirstSend = thread.messages().size();

    thread.sendMessage(QStringLiteral("second, while busy"), QString(), kFixtureExecutable);
    EXPECT_EQ(countAfterFirstSend, thread.messages().size());

    while (thread.isBusy())
      QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  }

  TEST_F(ChatThreadTest, BlankMessageIsANoOp) {
    chat::ChatThread thread(QStringLiteral("Thread 1"));

    thread.sendMessage(QStringLiteral("   "), QString(), kFixtureExecutable);

    EXPECT_FALSE(thread.isBusy());
    EXPECT_TRUE(thread.messages().empty());
  }

  TEST_F(ChatThreadTest, DefaultConstructorGeneratesAUniqueId) {
    chat::ChatThread first(QStringLiteral("Thread 1"));
    chat::ChatThread second(QStringLiteral("Thread 2"));

    EXPECT_FALSE(first.id().isEmpty());
    EXPECT_NE(first.id(), second.id());
  }

  TEST_F(ChatThreadTest, ExplicitIdConstructorKeepsTheGivenId) {
    chat::ChatThread thread(QStringLiteral("restored-id"), QStringLiteral("Restored"));

    EXPECT_EQ(QStringLiteral("restored-id"), thread.id());
    EXPECT_EQ(QStringLiteral("Restored"), thread.name());
  }

  TEST_F(ChatThreadTest, RestoreSetsSessionIdAndMessagesWithoutEmittingMessageAppended) {
    chat::ChatThread thread(QStringLiteral("restored-id"), QStringLiteral("Restored"));
    QSignalSpy appendedSpy(&thread, &chat::ChatThread::messageAppended);

    chat::ChatMessage userMessage;
    userMessage.role = chat::ChatMessageRole::User;
    userMessage.text = QStringLiteral("hello from disk");

    thread.restore(QStringLiteral("session-from-disk"), {userMessage});

    EXPECT_EQ(QStringLiteral("session-from-disk"), thread.sessionId());
    ASSERT_EQ(1u, thread.messages().size());
    EXPECT_EQ(QStringLiteral("hello from disk"), thread.messages()[0].text);
    EXPECT_EQ(0, appendedSpy.size());
  }

  TEST_F(ChatThreadTest, CliFailureAppendsAnErrorMessage) {
    chat::ChatThread thread(QStringLiteral("Thread 1"));

    thread.sendMessage(QStringLiteral("TRIGGER_FAILURE please"), QString(), kFixtureExecutable);
    while (thread.isBusy())
      QCoreApplication::processEvents(QEventLoop::AllEvents, 20);

    const auto& messages = thread.messages();
    EXPECT_EQ(chat::ChatMessageRole::Error, messages.back().role);
    EXPECT_EQ(QStringLiteral("simulated failure"), messages.back().text);
  }

}
