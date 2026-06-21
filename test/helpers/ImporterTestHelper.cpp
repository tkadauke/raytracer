#include "test/helpers/ImporterTestHelper.h"

#include "world/objects/Group.h"

#include <gtest/gtest.h>

#include <QDir>

namespace test::importers {

  ExpectedDiagnostic ExpectedDiagnostic::warning(const QString& message,
                                                 const QString& source,
                                                 int line,
                                                 int column) {
    return {world::ImportDiagnosticSeverity::Warning, message, source, line, column};
  }

  ExpectedDiagnostic ExpectedDiagnostic::error(const QString& message,
                                               const QString& source,
                                               int line,
                                               int column) {
    return {world::ImportDiagnosticSeverity::Error, message, source, line, column};
  }

  QString importerFixturePath(const QString& relativePath) {
    return QDir::current().absoluteFilePath(QString("test/fixtures/importers/%1").arg(relativePath));
  }

  void expectDiagnostics(const std::vector<world::ImportDiagnostic>& actual,
                         const std::vector<ExpectedDiagnostic>& expected) {
    ASSERT_EQ(expected.size(), actual.size());

    for (std::size_t i = 0; i < expected.size(); ++i) {
      EXPECT_EQ(expected[i].severity, actual[i].severity) << "diagnostic " << i;
      EXPECT_EQ(expected[i].message, actual[i].message) << "diagnostic " << i;
      EXPECT_EQ(expected[i].source, actual[i].source) << "diagnostic " << i;
      EXPECT_EQ(expected[i].line, actual[i].line) << "diagnostic " << i;
      EXPECT_EQ(expected[i].column, actual[i].column) << "diagnostic " << i;
    }
  }

  void expectGroupTree(const Group& actual, const ExpectedGroup& expected) {
    EXPECT_EQ(expected.name, actual.name());
    EXPECT_EQ(expected.visible, actual.visible()) << qPrintable(actual.name());

    for (auto it = expected.metadata.begin(); it != expected.metadata.end(); ++it) {
      ASSERT_TRUE(actual.metadata().contains(it.key()))
        << qPrintable(actual.name()) << " missing metadata key " << qPrintable(it.key());
      EXPECT_EQ(it.value(), actual.metadata().value(it.key()))
        << qPrintable(actual.name()) << " metadata key " << qPrintable(it.key());
    }

    const auto groupChildren = actual.childElementsOfType<Group>();

    ASSERT_EQ(expected.children.size(), groupChildren.size()) << qPrintable(actual.name());
    for (std::size_t i = 0; i < expected.children.size(); ++i)
      expectGroupTree(*groupChildren[i], expected.children[i]);
  }

}
