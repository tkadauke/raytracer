#include <gtest/gtest.h>

#include <QJsonObject>

#include "world/import/ImportDiagnostic.h"

namespace ImportDiagnosticTest {

  TEST(ImportDiagnostic, JsonRoundTripPreservesAllFields) {
    const auto original =
      world::ImportDiagnostic::error("invalid facet", "mesh.stl", 12, 4);

    const auto decoded = world::ImportDiagnostic::fromJson(original.toJson());

    EXPECT_TRUE(decoded.isError());
    EXPECT_EQ(QStringLiteral("error"), decoded.severityName());
    EXPECT_EQ(original.message, decoded.message);
    EXPECT_EQ(original.source, decoded.source);
    EXPECT_EQ(original.line, decoded.line);
    EXPECT_EQ(original.column, decoded.column);
  }

  TEST(ImportDiagnostic, JsonKeepsExistingWarningDefaults) {
    const auto decoded = world::ImportDiagnostic::fromJson(
      QJsonObject{{QStringLiteral("message"), QStringLiteral("unitless input")}});

    EXPECT_TRUE(decoded.isWarning());
    EXPECT_EQ(QStringLiteral("warning"), decoded.severityName());
    EXPECT_EQ(-1, decoded.line);
    EXPECT_EQ(-1, decoded.column);
    EXPECT_FALSE(decoded.toJson().contains(QStringLiteral("source")));
    EXPECT_FALSE(decoded.toJson().contains(QStringLiteral("line")));
    EXPECT_FALSE(decoded.toJson().contains(QStringLiteral("column")));
  }

}
