#include <gtest/gtest.h>

#include "src/modeler/RecentFileList.h"

#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>

namespace RecentFileListTest {
  class RecentFileListTest : public ::testing::Test {};

  // `RecentFileList` constructs its `QSettings` with an explicit
  // `(IniFormat, UserScope, org, app)` to bypass macOS cfprefsd. The
  // test seed has to use the same explicit form so seeded values land
  // in the file the production code will read.
  QSettings settingsForTest() {
    return QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Raytracer"),
                     QStringLiteral("Modeler"));
  }

  void configureSettingsPath(const QString& path) {
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, path);
    QSettings settings = settingsForTest();
    settings.clear();
  }

  QStringList recentFilesForCount(int count) {
    QStringList files;
    for (int i = 0; i != count; ++i)
      files << QStringLiteral("/tmp/modeler-recent-%1.json").arg(i);
    return files;
  }

  TEST_F(RecentFileListTest, ShouldLoadEmptyListWhenNoRecentFilesExist) {
    QTemporaryDir settingsPath;
    ASSERT_TRUE(settingsPath.isValid());
    configureSettingsPath(settingsPath.path());

    RecentFileList recentFiles;
    recentFiles.load();

    EXPECT_TRUE(recentFiles.files().isEmpty());
  }

  TEST_F(RecentFileListTest, ShouldLoadRecentFilesFromSettingsWithTenFileCap) {
    QTemporaryDir settingsPath;
    ASSERT_TRUE(settingsPath.isValid());
    configureSettingsPath(settingsPath.path());
    const QStringList files = recentFilesForCount(12);
    QSettings settings = settingsForTest();
    settings.setValue(QStringLiteral("recentOpenFiles"), files);
    settings.sync();

    RecentFileList recentFiles;
    recentFiles.load();

    ASSERT_EQ(RecentFileList::limit, recentFiles.files().size());
    EXPECT_EQ(files[0], recentFiles.files().front());
    EXPECT_EQ(files[9], recentFiles.files().back());
  }

  TEST_F(RecentFileListTest, ShouldAddFilesMostRecentFirstAndPersistThem) {
    QTemporaryDir settingsPath;
    ASSERT_TRUE(settingsPath.isValid());
    configureSettingsPath(settingsPath.path());

    RecentFileList recentFiles;
    recentFiles.add(QStringLiteral("/tmp/first.json"));
    recentFiles.add(QStringLiteral("/tmp/second.scad"));
    recentFiles.add(QStringLiteral("/tmp/first.json"));

    ASSERT_EQ(2, recentFiles.files().size());
    EXPECT_EQ(QFileInfo(QStringLiteral("/tmp/first.json")).absoluteFilePath(),
              recentFiles.files()[0]);
    EXPECT_EQ(QFileInfo(QStringLiteral("/tmp/second.scad")).absoluteFilePath(),
              recentFiles.files()[1]);

    RecentFileList reloaded;
    reloaded.load();
    EXPECT_EQ(recentFiles.files(), reloaded.files());
  }

  TEST_F(RecentFileListTest, ShouldRemoveFilesAndPersistRemoval) {
    QTemporaryDir settingsPath;
    ASSERT_TRUE(settingsPath.isValid());
    configureSettingsPath(settingsPath.path());

    RecentFileList recentFiles;
    recentFiles.add(QStringLiteral("/tmp/first.json"));
    recentFiles.add(QStringLiteral("/tmp/second.scad"));
    recentFiles.remove(QStringLiteral("/tmp/first.json"));

    ASSERT_EQ(1, recentFiles.files().size());
    EXPECT_EQ(QFileInfo(QStringLiteral("/tmp/second.scad")).absoluteFilePath(),
              recentFiles.files()[0]);

    RecentFileList reloaded;
    reloaded.load();
    EXPECT_EQ(recentFiles.files(), reloaded.files());
  }
} // namespace RecentFileListTest
