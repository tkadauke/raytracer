#pragma once

#include <QString>
#include <QStringList>

class RecentFileList {
public:
  static constexpr int limit = 10;

  RecentFileList();

  void load();
  void save() const;
  void add(const QString& fileName);
  void remove(const QString& fileName);

  [[nodiscard]] const QStringList& files() const;

private:
  void normalize();

  QStringList m_files;
};
