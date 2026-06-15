#include <gtest/gtest.h>

#include "core/json/JsonValue.h"

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
