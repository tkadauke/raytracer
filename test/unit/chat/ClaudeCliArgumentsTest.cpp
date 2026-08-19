#include <gtest/gtest.h>

#include <QString>
#include <QStringList>

#include "chat/ClaudeCliArguments.h"

namespace ClaudeCliArgumentsTest {

  TEST(ClaudeCliArgumentsTest, NewSessionOmitsResume) {
    chat::ClaudeCliRequest request;
    request.message = QStringLiteral("add a red sphere");
    request.mcpConfigPath = QStringLiteral("/tmp/mcp-config.json");

    const QStringList arguments = chat::claudeCliArguments(request);

    EXPECT_EQ((QStringList{QStringLiteral("-p"), QStringLiteral("add a red sphere"),
                           QStringLiteral("--output-format"), QStringLiteral("stream-json"),
                           QStringLiteral("--input-format"), QStringLiteral("stream-json"),
                           QStringLiteral("--mcp-config"), QStringLiteral("/tmp/mcp-config.json")}),
              arguments);
  }

  TEST(ClaudeCliArgumentsTest, ContinuingThreadAppendsResumeWithTheSessionId) {
    chat::ClaudeCliRequest request;
    request.message = QStringLiteral("now make it blue");
    request.mcpConfigPath = QStringLiteral("/tmp/mcp-config.json");
    request.resumeSessionId = QStringLiteral("session-123");

    const QStringList arguments = chat::claudeCliArguments(request);

    ASSERT_EQ(10, arguments.size());
    EXPECT_EQ(QStringLiteral("--resume"), arguments[8]);
    EXPECT_EQ(QStringLiteral("session-123"), arguments[9]);
  }

  TEST(ClaudeCliArgumentsTest, EmptyMcpConfigPathOmitsTheFlagEntirely) {
    chat::ClaudeCliRequest request;
    request.message = QStringLiteral("hello");

    const QStringList arguments = chat::claudeCliArguments(request);

    EXPECT_FALSE(arguments.contains(QStringLiteral("--mcp-config")));
  }

  TEST(ClaudeCliArgumentsTest, MessageIsPassedThroughVerbatimAsASingleArgument) {
    chat::ClaudeCliRequest request;
    request.message = QStringLiteral("multi word message with \"quotes\"");

    const QStringList arguments = chat::claudeCliArguments(request);

    EXPECT_EQ(request.message, arguments[1]);
  }

}
