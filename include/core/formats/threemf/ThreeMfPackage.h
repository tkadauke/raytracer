#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <map>
#include <stdexcept>

namespace core::threemf {

  inline QString normalizedPartName(QString name) {
    name.replace('\\', '/');
    while (name.startsWith('/'))
      name.remove(0, 1);
    return name;
  }

  class ThreeMfPackageError : public std::runtime_error {
  public:
    explicit ThreeMfPackageError(const std::string& message);
  };

  /**
    * Minimal ZIP reader for 3MF packages.
    *
    * 3MF files are OPC ZIP containers. The importer only needs random access
    * to package parts, so this reader indexes the central directory and
    * extracts stored or deflated entries by normalized part name.
    */
  class ThreeMfPackage {
  public:
    [[nodiscard]] static ThreeMfPackage read(const QString& filename);

    [[nodiscard]] bool contains(const QString& partName) const;
    [[nodiscard]] QByteArray part(const QString& partName) const;
    [[nodiscard]] QStringList partNames() const;

    /**
      * @returns the part name of this package's 3D model entry, resolved via
      *   the OPC relationship pointing at "/3dmodel", falling back to the
      *   conventional "3D/3dmodel.model" path, and then to any part ending
      *   in ".model".
      * @throws ThreeMfPackageError if no model part can be found.
      */
    [[nodiscard]] QString modelPartName() const;

  private:
    std::map<QString, QByteArray> m_parts;
  };

}
