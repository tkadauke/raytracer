#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>

#include "chat/ChatThreadStore.h"

namespace ChatThreadStoreTest {

  class ChatThreadStoreTest : public ::testing::Test {};

  namespace {
    chat::ChatMessage userMessage(const QString& text) {
      chat::ChatMessage message;
      message.role = chat::ChatMessageRole::User;
      message.text = text;
      return message;
    }

    chat::ChatMessage toolCallMessage() {
      chat::ChatMessage message;
      message.role = chat::ChatMessageRole::ToolCall;
      message.toolUseId = QStringLiteral("tool-1");
      message.toolName = QStringLiteral("query_scene");
      QJsonObject input;
      input[QStringLiteral("depth")] = 2;
      message.toolInput = input;
      return message;
    }

    chat::ChatMessage toolResultMessage(bool isError) {
      chat::ChatMessage message;
      message.role = chat::ChatMessageRole::ToolResult;
      message.toolUseId = QStringLiteral("tool-1");
      message.toolName = QStringLiteral("query_scene");
      message.text = QStringLiteral("{}");
      message.toolIsError = isError;
      return message;
    }
  }

  TEST_F(ChatThreadStoreTest, LoadThreadsOnAnUnknownSceneReturnsEmpty) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    chat::ChatThreadStore store(dir.path());

    EXPECT_TRUE(store.loadThreads(QStringLiteral("no-such-scene")).empty());
  }

  TEST_F(ChatThreadStoreTest, SaveThenLoadRoundTripsAllMessageFields) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    chat::ChatThreadStore store(dir.path());

    chat::ChatThreadRecord record;
    record.id = QStringLiteral("thread-1");
    record.name = QStringLiteral("Scene Cleanup");
    record.sessionId = QStringLiteral("session-abc");
    record.messages = {userMessage(QStringLiteral("add a red sphere")), toolCallMessage(),
                       toolResultMessage(false)};

    ASSERT_TRUE(store.saveThread(QStringLiteral("scene-1"), record));

    const auto loaded = store.loadThreads(QStringLiteral("scene-1"));
    ASSERT_EQ(1u, loaded.size());
    EXPECT_EQ(record.id, loaded[0].id);
    EXPECT_EQ(record.name, loaded[0].name);
    EXPECT_EQ(record.sessionId, loaded[0].sessionId);
    ASSERT_EQ(3u, loaded[0].messages.size());

    EXPECT_EQ(chat::ChatMessageRole::User, loaded[0].messages[0].role);
    EXPECT_EQ(QStringLiteral("add a red sphere"), loaded[0].messages[0].text);

    EXPECT_EQ(chat::ChatMessageRole::ToolCall, loaded[0].messages[1].role);
    EXPECT_EQ(QStringLiteral("tool-1"), loaded[0].messages[1].toolUseId);
    EXPECT_EQ(QStringLiteral("query_scene"), loaded[0].messages[1].toolName);
    EXPECT_EQ(2, loaded[0].messages[1].toolInput.value(QStringLiteral("depth")).toInt());

    EXPECT_EQ(chat::ChatMessageRole::ToolResult, loaded[0].messages[2].role);
    EXPECT_FALSE(loaded[0].messages[2].toolIsError);
  }

  TEST_F(ChatThreadStoreTest, SaveThreadOverwritesAPreviouslySavedRecordWithTheSameId) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    chat::ChatThreadStore store(dir.path());

    chat::ChatThreadRecord record;
    record.id = QStringLiteral("thread-1");
    record.name = QStringLiteral("Before Rename");
    store.saveThread(QStringLiteral("scene-1"), record);

    record.name = QStringLiteral("After Rename");
    record.messages = {userMessage(QStringLiteral("hello"))};
    store.saveThread(QStringLiteral("scene-1"), record);

    const auto loaded = store.loadThreads(QStringLiteral("scene-1"));
    ASSERT_EQ(1u, loaded.size());
    EXPECT_EQ(QStringLiteral("After Rename"), loaded[0].name);
    ASSERT_EQ(1u, loaded[0].messages.size());
  }

  TEST_F(ChatThreadStoreTest, ThreadsAreScopedToTheirScene) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    chat::ChatThreadStore store(dir.path());

    chat::ChatThreadRecord record;
    record.id = QStringLiteral("thread-1");
    record.name = QStringLiteral("Scene A Thread");
    store.saveThread(QStringLiteral("scene-a"), record);

    EXPECT_EQ(1u, store.loadThreads(QStringLiteral("scene-a")).size());
    EXPECT_TRUE(store.loadThreads(QStringLiteral("scene-b")).empty());
  }

  TEST_F(ChatThreadStoreTest, DeleteThreadRemovesItsFileButLeavesSiblingsInPlace) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    chat::ChatThreadStore store(dir.path());

    chat::ChatThreadRecord first;
    first.id = QStringLiteral("thread-1");
    first.name = QStringLiteral("Keep Me");
    store.saveThread(QStringLiteral("scene-1"), first);

    chat::ChatThreadRecord second;
    second.id = QStringLiteral("thread-2");
    second.name = QStringLiteral("Delete Me");
    store.saveThread(QStringLiteral("scene-1"), second);

    ASSERT_TRUE(store.deleteThread(QStringLiteral("scene-1"), QStringLiteral("thread-2")));

    const auto loaded = store.loadThreads(QStringLiteral("scene-1"));
    ASSERT_EQ(1u, loaded.size());
    EXPECT_EQ(QStringLiteral("Keep Me"), loaded[0].name);
  }

  TEST_F(ChatThreadStoreTest, DeleteThreadOnAMissingFileStillReturnsTrue) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    chat::ChatThreadStore store(dir.path());

    EXPECT_TRUE(store.deleteThread(QStringLiteral("scene-1"), QStringLiteral("never-existed")));
  }

  TEST_F(ChatThreadStoreTest, LoadThreadsSkipsAMalformedFileInsteadOfFailingTheWholeLoad) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    chat::ChatThreadStore store(dir.path());

    chat::ChatThreadRecord good;
    good.id = QStringLiteral("thread-good");
    good.name = QStringLiteral("Good Thread");
    store.saveThread(QStringLiteral("scene-1"), good);

    QDir sceneDir(dir.path() + QStringLiteral("/scene-1"));
    QFile bad(sceneDir.filePath(QStringLiteral("thread-bad.json")));
    ASSERT_TRUE(bad.open(QIODevice::WriteOnly));
    bad.write("not json{{{");
    bad.close();

    const auto loaded = store.loadThreads(QStringLiteral("scene-1"));
    ASSERT_EQ(1u, loaded.size());
    EXPECT_EQ(QStringLiteral("Good Thread"), loaded[0].name);
  }

}
