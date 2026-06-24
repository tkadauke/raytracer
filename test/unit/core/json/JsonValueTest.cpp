#include <gtest/gtest.h>

#include "core/json/JsonValue.h"

#include <stdexcept>

TEST(JsonValueTest, ConvertsVector3ToAndFromJsonArray) {
  const Vector3d vector(1.5, -2.0, 3.25);

  const QJsonArray json = core::json::vector3ToJsonArray(vector);

  ASSERT_EQ(3, json.size());
  EXPECT_DOUBLE_EQ(1.5, json[0].toDouble());
  EXPECT_DOUBLE_EQ(-2.0, json[1].toDouble());
  EXPECT_DOUBLE_EQ(3.25, json[2].toDouble());
  EXPECT_EQ(vector, core::json::vector3FromJsonArray(json));
}

TEST(JsonValueTest, ConvertsColorToAndFromJsonArray) {
  const Colord color(0.25, 0.5, 0.75);

  const QJsonArray json = core::json::colorToJsonArray(color);

  ASSERT_EQ(3, json.size());
  EXPECT_DOUBLE_EQ(0.25, json[0].toDouble());
  EXPECT_DOUBLE_EQ(0.5, json[1].toDouble());
  EXPECT_DOUBLE_EQ(0.75, json[2].toDouble());
  EXPECT_EQ(color, core::json::colorFromJsonArray(json));
}

TEST(JsonValueTest, RequiresNumberArrayWithExpectedSize) {
  const QJsonValue value(QJsonArray{1.0, 2.0, 3.0});

  const QJsonArray result = core::json::requireNumberArray(
    value, 3, "not-array", "wrong-size", "not-number",
    [](std::optional<int>, const char*) { throw std::runtime_error("unexpected error"); });

  ASSERT_EQ(3, result.size());
  EXPECT_DOUBLE_EQ(1.0, result[0].toDouble());
  EXPECT_DOUBLE_EQ(2.0, result[1].toDouble());
  EXPECT_DOUBLE_EQ(3.0, result[2].toDouble());
}

TEST(JsonValueTest, RequiresTypedNumberArrayWithExpectedSize) {
  const QJsonValue value(QJsonArray{1.0, 2.0, 3.0});

  const auto result = core::json::requireNumberArray<3>(
    value, "not-array", "wrong-size", "not-number",
    [](std::optional<int>, const char*) { throw std::runtime_error("unexpected error"); });

  ASSERT_TRUE(result.has_value());
  EXPECT_DOUBLE_EQ(1.0, result->at(0));
  EXPECT_DOUBLE_EQ(2.0, result->at(1));
  EXPECT_DOUBLE_EQ(3.0, result->at(2));
}

TEST(JsonValueTest, ReportsNonNumericArrayIndex) {
  const QJsonValue value(QJsonArray{1.0, "two", 3.0});
  std::optional<int> failedIndex;
  const char* failedMessage = nullptr;

  core::json::requireNumberArray(value, 3, "not-array", "wrong-size", "not-number",
                                 [&](std::optional<int> index, const char* message) {
                                   failedIndex = index;
                                   failedMessage = message;
                                 });

  ASSERT_TRUE(failedIndex.has_value());
  EXPECT_EQ(1, *failedIndex);
  EXPECT_STREQ("not-number", failedMessage);
}
