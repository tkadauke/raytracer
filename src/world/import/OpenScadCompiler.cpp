#include "world/import/OpenScadCompiler.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QStandardPaths>

namespace {
  QJsonObject cacheIdentityOptions(QJsonObject options) {
    options.remove("executable");
    options.remove("cacheDirectory");
    return options;
  }

  QString normalizedOutputFormat(const QString& outputFormat) {
    const QString format = outputFormat.trimmed().toLower();
    return format.isEmpty() ? QStringLiteral("stl") : format;
  }

  QStringList argumentsFor(const QString& outputPath, const QString& sourcePath,
                           const QJsonObject& options) {
    QStringList arguments;
    arguments << "-o" << outputPath;

    const auto defines = options.value("define").toObject();
    for (auto it = defines.begin(); it != defines.end(); ++it) {
      arguments << "-D" << QString("%1=%2").arg(it.key(), it.value().toVariant().toString());
    }

    arguments << sourcePath;
    return arguments;
  }
}

namespace world {

  OpenScadProcess::~OpenScadProcess() = default;

  int OpenScadProcess::run(const QString& executable, const QStringList& arguments,
                           const QString& workingDirectory, QString* standardOutput,
                           QString* standardError) const {
    QProcess process;
    process.setProgram(executable);
    process.setArguments(arguments);
    if (!workingDirectory.isEmpty())
      process.setWorkingDirectory(workingDirectory);
    process.start();
    if (!process.waitForStarted()) {
      if (standardError)
        *standardError = process.errorString();
      return -1;
    }
    process.waitForFinished(-1);
    if (standardOutput)
      *standardOutput = QString::fromLocal8Bit(process.readAllStandardOutput());
    if (standardError)
      *standardError = QString::fromLocal8Bit(process.readAllStandardError());
    return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
  }

  OpenScadCompiler::OpenScadCompiler(const OpenScadProcess* process)
      : m_process(process) {
  }

  OpenScadCompileResult OpenScadCompiler::compile(const OpenScadCompileRequest& request) const {
    OpenScadCompileResult result;
    result.cacheKey = cacheKeyFor(request.sourcePath, request.options);

    const QString cacheDirectory =
      request.cacheDirectory.trimmed().isEmpty() ? defaultCacheDirectory() : request.cacheDirectory;
    QDir().mkpath(cacheDirectory);
    result.outputPath =
      QDir(cacheDirectory)
        .filePath(result.cacheKey + "." + normalizedOutputFormat(request.outputFormat));

    if (QFileInfo::exists(result.outputPath)) {
      result.succeeded = true;
      result.cacheHit = true;
      return result;
    }

    const std::optional<QString> executable = findExecutable(request.executablePath);
    if (!executable) {
      result.diagnostics.push_back(ImportDiagnostic::warning(
        "OpenSCAD executable was not found; install openscad or set the executable import option",
        request.sourcePath));
      return result;
    }

    const OpenScadProcess defaultProcess;
    const OpenScadProcess& process = m_process ? *m_process : defaultProcess;
    QString standardOutput;
    QString standardError;
    const int exitCode =
      process.run(*executable, argumentsFor(result.outputPath, request.sourcePath, request.options),
                  QFileInfo(request.sourcePath).absolutePath(), &standardOutput, &standardError);
    if (exitCode != 0) {
      const QString detail =
        standardError.trimmed().isEmpty() ? standardOutput.trimmed() : standardError.trimmed();
      const QString summary = exitCode < 0
                                ? QStringLiteral("OpenSCAD crashed or failed to start")
                                : QString("OpenSCAD failed with exit code %1").arg(exitCode);
      result.diagnostics.push_back(ImportDiagnostic::error(
        detail.isEmpty() ? summary : QString("%1: %2").arg(summary, detail), request.sourcePath));
      QFile::remove(result.outputPath);
      return result;
    }

    if (!QFileInfo::exists(result.outputPath)) {
      result.diagnostics.push_back(ImportDiagnostic::error(
        "OpenSCAD completed without producing a mesh output", request.sourcePath));
      return result;
    }

    result.succeeded = true;
    result.cacheHit = false;
    return result;
  }

  std::optional<QString> OpenScadCompiler::findExecutable(const QString& overridePath) {
    if (!overridePath.trimmed().isEmpty()) {
      const QFileInfo info(overridePath);
      if (info.exists() && info.isExecutable())
        return info.absoluteFilePath();
      return std::nullopt;
    }

    const QString executable = QStandardPaths::findExecutable("openscad");
    if (executable.isEmpty())
      return std::nullopt;
    return executable;
  }

  QString OpenScadCompiler::cacheKeyFor(const QString& sourcePath, const QJsonObject& options) {
    QFile file(sourcePath);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (file.open(QIODevice::ReadOnly))
      hash.addData(&file);
    else
      hash.addData(sourcePath.toUtf8());
    hash.addData(QJsonDocument(cacheIdentityOptions(options)).toJson(QJsonDocument::Compact));
    return QString::fromLatin1(hash.result().toHex());
  }

  QString OpenScadCompiler::defaultCacheDirectory() {
    QString location = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (location.isEmpty())
      location = QDir::temp().filePath("raytracer-cache");
    return QDir(location).filePath("openscad");
  }

}
