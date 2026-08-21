#include <gtest/gtest.h>

#include <QString>
#include <QStringList>

#include "chat/ClaudeCliArguments.h"

namespace ClaudeCliArgumentsTest {

  namespace {
    const QString kExpectedAllowedTools = QStringLiteral(
      "mcp__raytracer-modeler__query_scene,mcp__raytracer-modeler__add_primitive,"
      "mcp__raytracer-modeler__transform,mcp__raytracer-modeler__apply_material,"
      "mcp__raytracer-modeler__select,mcp__raytracer-modeler__delete,"
      "mcp__raytracer-modeler__csg_union,mcp__raytracer-modeler__csg_intersect,"
      "mcp__raytracer-modeler__csg_difference,mcp__raytracer-modeler__set_camera");
  }

  TEST(ClaudeCliArgumentsTest, NewSessionOmitsResume) {
    chat::ClaudeCliRequest request;
    request.message = QStringLiteral("add a red sphere");
    request.mcpConfigPath = QStringLiteral("/tmp/mcp-config.json");

    const QStringList arguments = chat::claudeCliArguments(request);

    EXPECT_EQ((QStringList{QStringLiteral("-p"), QStringLiteral("add a red sphere"),
                           QStringLiteral("--output-format"), QStringLiteral("stream-json"),
                           QStringLiteral("--verbose"), QStringLiteral("--mcp-config"),
                           QStringLiteral("/tmp/mcp-config.json"), QStringLiteral("--allowedTools"),
                           kExpectedAllowedTools}),
              arguments);
  }

  TEST(ClaudeCliArgumentsTest, ContinuingThreadAppendsResumeWithTheSessionId) {
    chat::ClaudeCliRequest request;
    request.message = QStringLiteral("now make it blue");
    request.mcpConfigPath = QStringLiteral("/tmp/mcp-config.json");
    request.resumeSessionId = QStringLiteral("session-123");

    const QStringList arguments = chat::claudeCliArguments(request);

    ASSERT_EQ(11, arguments.size());
    EXPECT_EQ(QStringLiteral("--resume"), arguments[9]);
    EXPECT_EQ(QStringLiteral("session-123"), arguments[10]);
  }

  TEST(ClaudeCliArgumentsTest, EmptyMcpConfigPathOmitsTheFlagEntirely) {
    chat::ClaudeCliRequest request;
    request.message = QStringLiteral("hello");

    const QStringList arguments = chat::claudeCliArguments(request);

    EXPECT_FALSE(arguments.contains(QStringLiteral("--mcp-config")));
  }

  TEST(ClaudeCliArgumentsTest, EmptyMcpConfigPathOmitsAllowedToolsToo) {
    chat::ClaudeCliRequest request;
    request.message = QStringLiteral("hello");

    const QStringList arguments = chat::claudeCliArguments(request);

    EXPECT_FALSE(arguments.contains(QStringLiteral("--allowedTools")));
  }

  TEST(ClaudeCliArgumentsTest, AllowedToolsScopedToOurTenMcpToolsOnly) {
    chat::ClaudeCliRequest request;
    request.message = QStringLiteral("add a red sphere");
    request.mcpConfigPath = QStringLiteral("/tmp/mcp-config.json");

    const QStringList arguments = chat::claudeCliArguments(request);

    const int flagIndex = arguments.indexOf(QStringLiteral("--allowedTools"));
    ASSERT_NE(-1, flagIndex);
    ASSERT_LT(flagIndex + 1, arguments.size());
    EXPECT_EQ(kExpectedAllowedTools, arguments[flagIndex + 1]);
  }

  TEST(ClaudeCliArgumentsTest, MessageIsPassedThroughVerbatimAsASingleArgument) {
    chat::ClaudeCliRequest request;
    request.message = QStringLiteral("multi word message with \"quotes\"");

    const QStringList arguments = chat::claudeCliArguments(request);

    EXPECT_EQ(request.message, arguments[1]);
  }

}
