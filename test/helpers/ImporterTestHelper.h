#pragma once

#include "world/import/ImportDiagnostic.h"

#include <QString>
#include <QJsonObject>

#include <vector>

class Element;
class Group;

namespace test::importers {

  struct ExpectedDiagnostic {
    world::ImportDiagnosticSeverity severity{world::ImportDiagnosticSeverity::Warning};
    QString message;
    QString source;
    int line{-1};
    int column{-1};

    [[nodiscard]] static ExpectedDiagnostic warning(const QString& message,
                                                    const QString& source = QString(),
                                                    int line = -1, int column = -1);
    [[nodiscard]] static ExpectedDiagnostic error(const QString& message,
                                                  const QString& source = QString(),
                                                  int line = -1, int column = -1);
  };

  struct ExpectedGroup {
    QString name;
    bool visible{true};
    QJsonObject metadata;
    std::vector<ExpectedGroup> children;
  };

  [[nodiscard]] QString importerFixturePath(const QString& relativePath);

  void expectDiagnostics(const std::vector<world::ImportDiagnostic>& actual,
                         const std::vector<ExpectedDiagnostic>& expected);

  void expectGroupTree(const Group& actual, const ExpectedGroup& expected);

  Group* childGroup(Element* parent, int index);

}
