#include <gtest/gtest.h>

#include <QByteArray>
#include <QJsonObject>
#include <QVector>

#include "chat/StreamJsonLineParser.h"

namespace StreamJsonLineParserTest {

  TEST(StreamJsonLineParserTest, ParsesASingleCompleteLine) {
    chat::StreamJsonLineParser parser;

    const auto events = parser.feed(QByteArrayLiteral("{\"type\":\"system\"}\n"));

    ASSERT_EQ(1, events.size());
    EXPECT_EQ(QStringLiteral("system"), events[0][QStringLiteral("type")].toString());
  }

  TEST(StreamJsonLineParserTest, ParsesMultipleLinesInOneChunk) {
    chat::StreamJsonLineParser parser;

    const auto events =
      parser.feed(QByteArrayLiteral("{\"type\":\"a\"}\n{\"type\":\"b\"}\n{\"type\":\"c\"}\n"));

    ASSERT_EQ(3, events.size());
    EXPECT_EQ(QStringLiteral("a"), events[0][QStringLiteral("type")].toString());
    EXPECT_EQ(QStringLiteral("b"), events[1][QStringLiteral("type")].toString());
    EXPECT_EQ(QStringLiteral("c"), events[2][QStringLiteral("type")].toString());
  }

  TEST(StreamJsonLineParserTest, HoldsBackAPartialTrailingLineUntilItCompletes) {
    chat::StreamJsonLineParser parser;

    EXPECT_TRUE(parser.feed(QByteArrayLiteral("{\"type\":\"asse")).isEmpty());
    const auto events = parser.feed(QByteArrayLiteral("mbled\"}\n"));

    ASSERT_EQ(1, events.size());
    EXPECT_EQ(QStringLiteral("assembled"), events[0][QStringLiteral("type")].toString());
  }

  TEST(StreamJsonLineParserTest, HandlesALineSplitAcrossManyTinyChunks) {
    chat::StreamJsonLineParser parser;

    const QByteArray line = QByteArrayLiteral("{\"type\":\"chunked\"}\n");
    QVector<QJsonObject> events;
    for (char byte : line) {
      const auto parsed = parser.feed(QByteArray(1, byte));
      events += parsed;
    }

    ASSERT_EQ(1, events.size());
    EXPECT_EQ(QStringLiteral("chunked"), events[0][QStringLiteral("type")].toString());
  }

  TEST(StreamJsonLineParserTest, SkipsBlankLines) {
    chat::StreamJsonLineParser parser;

    const auto events = parser.feed(QByteArrayLiteral("\n\n{\"type\":\"after-blanks\"}\n\n"));

    ASSERT_EQ(1, events.size());
    EXPECT_EQ(QStringLiteral("after-blanks"), events[0][QStringLiteral("type")].toString());
  }

  TEST(StreamJsonLineParserTest, SkipsMalformedOrNonObjectLinesWithoutWedging) {
    chat::StreamJsonLineParser parser;

    const auto events = parser.feed(
      QByteArrayLiteral("not json at all\n[1,2,3]\n\"just a string\"\n{\"type\":\"survivor\"}\n"));

    ASSERT_EQ(1, events.size());
    EXPECT_EQ(QStringLiteral("survivor"), events[0][QStringLiteral("type")].toString());
  }

  TEST(StreamJsonLineParserTest, FlushParsesATrailingUnterminatedLine) {
    chat::StreamJsonLineParser parser;

    EXPECT_TRUE(parser.feed(QByteArrayLiteral("{\"type\":\"no-trailing-newline\"}")).isEmpty());
    const auto events = parser.flush();

    ASSERT_EQ(1, events.size());
    EXPECT_EQ(QStringLiteral("no-trailing-newline"), events[0][QStringLiteral("type")].toString());
  }

  TEST(StreamJsonLineParserTest, FlushOnEmptyBufferReturnsNothing) {
    chat::StreamJsonLineParser parser;

    EXPECT_TRUE(parser.flush().isEmpty());
  }

}
