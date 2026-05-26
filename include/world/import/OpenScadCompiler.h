#pragma once

#include "world/import/ImportDiagnostic.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace world {

  struct OpenScadCompileRequest {
    QString sourcePath;
    QString executablePath;
    QString cacheDirectory;
    QString outputFormat;
    QJsonObject options;
  };

  struct OpenScadCompileResult {
    bool succeeded{false};
    bool cacheHit{false};
    QString outputPath;
    QString cacheKey;
    std::vector<ImportDiagnostic> diagnostics;
  };

  class OpenScadProcess {
  public:
    virtual ~OpenScadProcess();

    [[nodiscard]] virtual int run(const QString& executable, const QStringList& arguments,
                                  const QString& workingDirectory, QString* standardOutput,
                                  QString* standardError) const;
  };

  class OpenScadCompiler {
  public:
    explicit OpenScadCompiler(const OpenScadProcess* process = nullptr);

    [[nodiscard]] OpenScadCompileResult compile(const OpenScadCompileRequest& request) const;

    [[nodiscard]] static std::optional<QString> findExecutable(const QString& overridePath);
    [[nodiscard]] static QString cacheKeyFor(const QString& sourcePath, const QJsonObject& options);
    [[nodiscard]] static QString defaultCacheDirectory();

  private:
    const OpenScadProcess* m_process;
  };

}
