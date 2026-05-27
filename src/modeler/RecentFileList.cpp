#include "RecentFileList.h"

#include <QFileInfo>
#include <QSettings>

RecentFileList::RecentFileList() = default;

void RecentFileList::load() {
  QSettings settings(QStringLiteral("Raytracer"), QStringLiteral("Modeler"));
  m_files = settings.value(QStringLiteral("recentOpenFiles")).toStringList();
  normalize();
}

void RecentFileList::save() const {
  QSettings settings(QStringLiteral("Raytracer"), QStringLiteral("Modeler"));
  settings.setValue(QStringLiteral("recentOpenFiles"), m_files);
  settings.sync();
}

void RecentFileList::add(const QString& fileName) {
  const QString canonicalFileName = QFileInfo(fileName).absoluteFilePath();
  if (canonicalFileName.isEmpty())
    return;

  m_files.removeAll(canonicalFileName);
  m_files.prepend(canonicalFileName);
  normalize();
  save();
}

void RecentFileList::remove(const QString& fileName) {
  const QString canonicalFileName = QFileInfo(fileName).absoluteFilePath();
  m_files.removeAll(canonicalFileName);
  m_files.removeAll(fileName);
  normalize();
  save();
}

const QStringList& RecentFileList::files() const {
  return m_files;
}

void RecentFileList::normalize() {
  QStringList normalized;
  normalized.reserve(m_files.size());
  for (const auto& file : m_files) {
    if (file.isEmpty() || normalized.contains(file))
      continue;
    normalized << file;
    if (normalized.size() == limit)
      break;
  }
  m_files = normalized;
}
