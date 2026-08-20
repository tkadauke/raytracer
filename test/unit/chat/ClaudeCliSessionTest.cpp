#include <gtest/gtest.h>

#include <QJsonObject>
#include <QSignalSpy>
#include <QString>

#include "chat/ClaudeCliSession.h"

#include "test/helpers/GuiTestHelper.h"

namespace ClaudeCliSessionTest {

  namespace {
    const QString kFixtureExecutable = QStringLiteral("test/fixtures/claude/fake_claude.py");
  }

  class ClaudeCliSessionTest : public ::testing::GuiTest {
  protected:
    static chat::ClaudeCliRequest requestFor(const QString& message,
                                             const QString& resumeSessionId = QString()) {
      chat::ClaudeCliRequest request;
      request.message = message;
      request.executable = kFixtureExecutable;
      request.resumeSessionId = resumeSessionId;
      return request;
    }
  };

  TEST_F(ClaudeCliSessionTest, CapturesSessionIdAndStreamsAssistantTextAndToolEvents) {
    chat::ClaudeCliSession session;

    QSignalSpy sessionIdSpy(&session, &chat::ClaudeCliSession::sessionIdCaptured);
    QSignalSpy assistantTextSpy(&session, &chat::ClaudeCliSession::assistantText);
    QSignalSpy toolStartedSpy(&session, &chat::ClaudeCliSession::toolCallStarted);
    QSignalSpy toolFinishedSpy(&session, &chat::ClaudeCliSession::toolCallFinished);
    QSignalSpy finishedSpy(&session, &chat::ClaudeCliSession::finished);

    session.start(requestFor(QStringLiteral("add a red sphere")));

    ASSERT_TRUE(finishedSpy.wait(5000));

    ASSERT_EQ(1, sessionIdSpy.size());
    EXPECT_FALSE(sessionIdSpy[0][0].toString().isEmpty());
    EXPECT_EQ(sessionIdSpy[0][0].toString(), session.sessionId());

    ASSERT_EQ(1, assistantTextSpy.size());
    EXPECT_EQ(QStringLiteral("Echo: add a red sphere"), assistantTextSpy[0][0].toString());

    ASSERT_EQ(1, toolStartedSpy.size());
    EXPECT_EQ(QStringLiteral("toolu_1"), toolStartedSpy[0][0].toString());
    EXPECT_EQ(QStringLiteral("query_scene"), toolStartedSpy[0][1].toString());
    EXPECT_TRUE(toolStartedSpy[0][2].toJsonObject().isEmpty());

    ASSERT_EQ(1, toolFinishedSpy.size());
    EXPECT_EQ(QStringLiteral("toolu_1"), toolFinishedSpy[0][0].toString());
    EXPECT_EQ(QStringLiteral("{\"elements\": []}"), toolFinishedSpy[0][1].toString());
    EXPECT_FALSE(toolFinishedSpy[0][2].toBool());

    ASSERT_EQ(1, finishedSpy.size());
    EXPECT_TRUE(finishedSpy[0][0].toBool());
    EXPECT_TRUE(finishedSpy[0][1].toString().isEmpty());

    EXPECT_FALSE(session.isRunning());
  }

  TEST_F(ClaudeCliSessionTest, ResumingPassesTheSameSessionIdThrough) {
    chat::ClaudeCliSession session;
    QSignalSpy sessionIdSpy(&session, &chat::ClaudeCliSession::sessionIdCaptured);
    QSignalSpy finishedSpy(&session, &chat::ClaudeCliSession::finished);

    session.start(requestFor(QStringLiteral("continue please"), QStringLiteral("session-abc")));

    ASSERT_TRUE(finishedSpy.wait(5000));
    ASSERT_EQ(1, sessionIdSpy.size());
    EXPECT_EQ(QStringLiteral("session-abc"), sessionIdSpy[0][0].toString());
  }

  TEST_F(ClaudeCliSessionTest, SurfacesAResultLevelFailureFromTheCli) {
    chat::ClaudeCliSession session;
    QSignalSpy finishedSpy(&session, &chat::ClaudeCliSession::finished);

    session.start(requestFor(QStringLiteral("TRIGGER_FAILURE please")));

    ASSERT_TRUE(finishedSpy.wait(5000));
    ASSERT_EQ(1, finishedSpy.size());
    EXPECT_FALSE(finishedSpy[0][0].toBool());
    EXPECT_EQ(QStringLiteral("simulated failure"), finishedSpy[0][1].toString());
  }

  TEST_F(ClaudeCliSessionTest, ACrashBeforeAnyOutputStillReportsFinished) {
    chat::ClaudeCliSession session;
    QSignalSpy finishedSpy(&session, &chat::ClaudeCliSession::finished);

    session.start(requestFor(QStringLiteral("TRIGGER_CRASH please")));

    ASSERT_TRUE(finishedSpy.wait(5000));
    ASSERT_EQ(1, finishedSpy.size());
    EXPECT_FALSE(finishedSpy[0][0].toBool());
    // The subprocess's own stderr is more useful to an operator than a bare
    // "claude exited with code 2" — see ClaudeCliSession::handleProcessFinished.
    EXPECT_EQ(QStringLiteral("fatal: simulated crash trigger"), finishedSpy[0][1].toString());
  }

  TEST_F(ClaudeCliSessionTest, FailingToStartReportsFinishedWithoutHanging) {
    chat::ClaudeCliSession session;
    QSignalSpy finishedSpy(&session, &chat::ClaudeCliSession::finished);

    chat::ClaudeCliRequest request = requestFor(QStringLiteral("hello"));
    request.executable = QStringLiteral("test/fixtures/claude/does-not-exist");
    session.start(request);

    ASSERT_TRUE(finishedSpy.wait(5000));
    ASSERT_EQ(1, finishedSpy.size());
    EXPECT_FALSE(finishedSpy[0][0].toBool());
    EXPECT_FALSE(finishedSpy[0][1].toString().isEmpty());
  }

  TEST_F(ClaudeCliSessionTest, IsRunningWhileTheSubprocessIsInFlight) {
    chat::ClaudeCliSession session;
    QSignalSpy finishedSpy(&session, &chat::ClaudeCliSession::finished);

    EXPECT_FALSE(session.isRunning());
    session.start(requestFor(QStringLiteral("hello")));
    EXPECT_TRUE(session.isRunning());

    ASSERT_TRUE(finishedSpy.wait(5000));
    EXPECT_FALSE(session.isRunning());
  }

}
