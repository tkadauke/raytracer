#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <utility>
#include <vector>

namespace world {

  enum class ImportOptionType { Boolean, Integer, Double, String, FilePath, DirectoryPath, Choice };

  struct ImportOptionSchema {
    ImportOptionSchema() = default;

    ImportOptionSchema(QString name, ImportOptionType type, QString label, QString description,
                       QVariant defaultValue = QVariant(), bool required = false,
                       QStringList choices = {}, QString group = {}, QVariant minimum = QVariant(),
                       QVariant maximum = QVariant(), QVariant step = QVariant())
        : name(std::move(name)),
          type(type),
          label(std::move(label)),
          description(std::move(description)),
          defaultValue(std::move(defaultValue)),
          required(required),
          choices(std::move(choices)),
          group(std::move(group)),
          minimum(std::move(minimum)),
          maximum(std::move(maximum)),
          step(std::move(step)) {
    }

    QString name;
    ImportOptionType type{ImportOptionType::String};
    QString label;
    QString description;
    QVariant defaultValue;
    bool required{false};
    QStringList choices;
    QString group;
    QVariant minimum;
    QVariant maximum;
    QVariant step;
  };

  /**
    * Format-neutral option values supplied to a SceneImporter.
    *
    * The schema describes expected names and types; this class intentionally
    * stores the selected values as JSON-compatible variants so future importers
    * can add format-specific controls without changing the shared interface.
    */
  class ImportOptions {
  public:
    ImportOptions() = default;
    explicit ImportOptions(QJsonObject values);

    [[nodiscard]] bool contains(const QString& name) const;
    [[nodiscard]] QVariant value(const QString& name, const QVariant& fallback = QVariant()) const;

    void setValue(const QString& name, const QVariant& value);

    [[nodiscard]] const QJsonObject& values() const {
      return m_values;
    }

  private:
    QJsonObject m_values;
  };

  using ImportOptionSchemas = std::vector<ImportOptionSchema>;

}
