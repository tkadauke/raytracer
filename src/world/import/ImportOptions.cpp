#include "world/import/ImportOptions.h"

#include <QJsonValue>

#include <utility>

namespace world {

  ImportOptions::ImportOptions(QJsonObject values)
      : m_values(std::move(values)) {
  }

  bool ImportOptions::contains(const QString& name) const {
    return m_values.contains(name);
  }

  QVariant ImportOptions::value(const QString& name, const QVariant& fallback) const {
    const auto jsonValue = m_values.value(name);
    if (jsonValue.isUndefined())
      return fallback;
    return jsonValue.toVariant();
  }

  void ImportOptions::setValue(const QString& name, const QVariant& value) {
    m_values.insert(name, QJsonValue::fromVariant(value));
  }

}
