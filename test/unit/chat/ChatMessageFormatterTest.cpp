#include <gtest/gtest.h>

#include <QString>

#include "chat/ChatMessage.h"
#include "chat/ChatMessageFormatter.h"

namespace ChatMessageFormatterTest {

  TEST(ChatMessageFormatterTest, RendersUserMessage) {
    chat::ChatMessage message;
    message.role = chat::ChatMessageRole::User;
    message.text = QStringLiteral("add a sphere");

    const QString html = chat::formatChatMessageHtml(message);

    EXPECT_TRUE(html.contains(QStringLiteral("You:")));
    EXPECT_TRUE(html.contains(QStringLiteral("add a sphere")));
  }

  TEST(ChatMessageFormatterTest, RendersAssistantMessage) {
    chat::ChatMessage message;
    message.role = chat::ChatMessageRole::Assistant;
    message.text = QStringLiteral("Sure, adding it now.");

    const QString html = chat::formatChatMessageHtml(message);

    EXPECT_TRUE(html.contains(QStringLiteral("Claude:")));
    EXPECT_TRUE(html.contains(QStringLiteral("Sure, adding it now.")));
  }

  TEST(ChatMessageFormatterTest, EscapesUntrustedTextInsteadOfInjectingHtml) {
    chat::ChatMessage message;
    message.role = chat::ChatMessageRole::User;
    message.text = QStringLiteral("<script>alert(1)</script>");

    const QString html = chat::formatChatMessageHtml(message);

    EXPECT_FALSE(html.contains(QStringLiteral("<script>")));
    EXPECT_TRUE(html.contains(QStringLiteral("&lt;script&gt;")));
  }

  TEST(ChatMessageFormatterTest, RendersToolCallWithNameAndInput) {
    chat::ChatMessage message;
    message.role = chat::ChatMessageRole::ToolCall;
    message.toolName = QStringLiteral("add_primitive");
    QJsonObject input;
    input[QStringLiteral("type")] = QStringLiteral("Sphere");
    message.toolInput = input;

    const QString html = chat::formatChatMessageHtml(message);

    EXPECT_TRUE(html.contains(QStringLiteral("tool call")));
    EXPECT_TRUE(html.contains(QStringLiteral("add_primitive")));
    EXPECT_TRUE(html.contains(QStringLiteral("Sphere")));
  }

  TEST(ChatMessageFormatterTest, RendersSuccessfulToolResultDistinctFromError) {
    chat::ChatMessage success;
    success.role = chat::ChatMessageRole::ToolResult;
    success.toolName = QStringLiteral("query_scene");
    success.text = QStringLiteral("{}");
    success.toolIsError = false;

    chat::ChatMessage failure = success;
    failure.toolIsError = true;
    failure.text = QStringLiteral("no such element");

    const QString successHtml = chat::formatChatMessageHtml(success);
    const QString failureHtml = chat::formatChatMessageHtml(failure);

    EXPECT_TRUE(successHtml.contains(QStringLiteral("tool result")));
    EXPECT_FALSE(successHtml.contains(QStringLiteral("tool error")));

    EXPECT_TRUE(failureHtml.contains(QStringLiteral("tool error")));
    EXPECT_TRUE(failureHtml.contains(QStringLiteral("no such element")));
  }

  TEST(ChatMessageFormatterTest, RendersErrorMessage) {
    chat::ChatMessage message;
    message.role = chat::ChatMessageRole::Error;
    message.text = QStringLiteral("claude exited with code 1");

    const QString html = chat::formatChatMessageHtml(message);

    EXPECT_TRUE(html.contains(QStringLiteral("Error:")));
    EXPECT_TRUE(html.contains(QStringLiteral("claude exited with code 1")));
  }

}
