#include <gtest/gtest.h>

#include "world/objects/Scene.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/Material.h"
#include "world/objects/Texture.h"
#include "raytracer/primitives/Scene.h"
#include "core/math/Vector.h"
#include "core/math/Angle.h"
#include "core/Color.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>
#include <QUuid>

// Q_DECLARE_METATYPE expands to a per-TU QMetaTypeId<T> specialisation;
// each TU that calls qRegisterMetaType<T> needs to see it. The library
// declares these in Element.cpp; mirror them here so the test TU's
// MetaTypeRegistrar below compiles.
Q_DECLARE_METATYPE(Vector3d);
Q_DECLARE_METATYPE(Angled);
Q_DECLARE_METATYPE(Colord);

namespace SceneTest {
  // Register the custom metatypes the world::* Q_PROPERTYs use. Without
  // this, QMetaProperty::read silently returns an invalid QVariant for
  // Vector3d/Colord/Angled/Material*/Texture* properties, so JSON
  // roundtrip drops them. The production GUI app and rendercli register
  // these at startup (see tools/rendercli/rendercli.cpp ~L224); the test
  // binary doesn't link those entry points, so we mirror the calls here.
  // TODO: lift this into a library-level Init() so apps and tests don't
  // each redo it (latent app/library coupling — see #24 follow-up).
  struct MetaTypeRegistrar {
    MetaTypeRegistrar() {
      qRegisterMetaType<Vector3d>();
      qRegisterMetaType<Angled>();
      qRegisterMetaType<Colord>();
      qRegisterMetaType<Material*>();
      qRegisterMetaType<Texture*>();
    }
  };
  static const MetaTypeRegistrar s_registrar;
  TEST(Scene, ShouldDefaultToCannedNewSceneName) {
    Scene scene;
    EXPECT_EQ(QString("New Scene"), scene.name());
  }

  TEST(Scene, ShouldDefaultToWarmAmbient) {
    // (0.4, 0.4, 0.4) is the canned default the constructor sets — bright
    // enough that an unlit scene isn't pitch-black during scene editing.
    Scene scene;
    EXPECT_EQ(Colord(0.4, 0.4, 0.4), scene.ambient());
  }

  TEST(Scene, ShouldDefaultToSkyBlueBackground) {
    Scene scene;
    EXPECT_EQ(Colord(0.4, 0.8, 1), scene.background());
  }

  TEST(Scene, ShouldDefaultToUnchanged) {
    Scene scene;
    EXPECT_FALSE(scene.changed());
  }

  TEST(Scene, ShouldSetAndGetAmbient) {
    Scene scene;
    scene.setAmbient(Colord(0.1, 0.2, 0.3));
    EXPECT_EQ(Colord(0.1, 0.2, 0.3), scene.ambient());
  }

  TEST(Scene, ShouldSetAndGetBackground) {
    Scene scene;
    scene.setBackground(Colord(0.5, 0.6, 0.7));
    EXPECT_EQ(Colord(0.5, 0.6, 0.7), scene.background());
  }

  TEST(Scene, ShouldSetAndGetChanged) {
    Scene scene;
    scene.setChanged(true);
    EXPECT_TRUE(scene.changed());
  }

  TEST(Scene, ShouldAcceptAnyChild) {
    // Scene::canHaveChild returns true unconditionally — it's the root of
    // the editable hierarchy and has to take cameras, lights, surfaces,
    // textures, materials. The bare Element child here is the lightest
    // proof that the predicate doesn't discriminate.
    Scene scene;
    Element child;
    EXPECT_TRUE(scene.canHaveChild(&child));
  }

  TEST(Scene, ShouldReturnNullActiveCameraWhenEmpty) {
    Scene scene;
    EXPECT_EQ(nullptr, scene.activeCamera());
  }

  TEST(Scene, ShouldReturnSingleCameraAsActive) {
    Scene scene;
    auto* camera = new PinholeCamera;
    scene.addChild(camera);
    EXPECT_EQ(camera, scene.activeCamera());
  }

  TEST(Scene, ShouldReturnLastCameraAsActiveWhenMultiple) {
    // activeCamera() walks all children and keeps the last camera it sees,
    // so the most-recently-added camera wins. Documenting that here so a
    // future change that picks the *first* camera (e.g. for stability) is
    // a deliberate behaviour change rather than an accident.
    Scene scene;
    auto* first = new PinholeCamera;
    auto* second = new PinholeCamera;
    scene.addChild(first);
    scene.addChild(second);
    EXPECT_EQ(second, scene.activeCamera());
  }

  TEST(Scene, ShouldIgnoreNonCameraChildrenInActiveCamera) {
    Scene scene;
    auto* nonCamera = new Element;
    scene.addChild(nonCamera);
    EXPECT_EQ(nullptr, scene.activeCamera());
  }

  TEST(Scene, ShouldRoundtripAmbientAndBackgroundViaSaveLoad) {
    QTemporaryFile temp;
    ASSERT_TRUE(temp.open());
    auto path = temp.fileName();
    temp.close();

    Scene original;
    original.setAmbient(Colord(0.1, 0.2, 0.3));
    original.setBackground(Colord(0.4, 0.5, 0.6));
    ASSERT_TRUE(original.save(path));

    Scene decoded;
    ASSERT_TRUE(decoded.load(path));
    EXPECT_EQ(Colord(0.1, 0.2, 0.3), decoded.ambient());
    EXPECT_EQ(Colord(0.4, 0.5, 0.6), decoded.background());

    QFile::remove(path);
  }

  TEST(Scene, ShouldClearChangedFlagOnSave) {
    QTemporaryFile temp;
    ASSERT_TRUE(temp.open());
    auto path = temp.fileName();
    temp.close();

    Scene scene;
    scene.setChanged(true);
    ASSERT_TRUE(scene.save(path));
    EXPECT_FALSE(scene.changed());

    QFile::remove(path);
  }

  TEST(Scene, ShouldFailLoadOnMissingFile) {
    Scene scene;
    auto path = QDir::tempPath() + "/raytracer-scene-missing-" +
                QUuid::createUuid().toString(QUuid::WithoutBraces);
    EXPECT_FALSE(scene.load(path));
  }

  TEST(Scene, ShouldFailSaveOnUnwritablePath) {
    Scene scene;
    // / is non-writable on all reasonable test environments (and the
    // file's name guarantees no collision); save returns false rather
    // than throwing.
    EXPECT_FALSE(scene.save("/raytracer-scene-unwritable-path.json"));
  }

  TEST(Scene, ShouldWriteValidJsonFile) {
    QTemporaryFile temp;
    ASSERT_TRUE(temp.open());
    auto path = temp.fileName();
    temp.close();

    Scene scene;
    ASSERT_TRUE(scene.save(path));

    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    auto bytes = file.readAll();
    file.close();

    auto doc = QJsonDocument::fromJson(bytes);
    ASSERT_FALSE(doc.isNull());
    ASSERT_TRUE(doc.isObject());
    EXPECT_EQ(QString("Scene"), doc.object()["type"].toString());

    QFile::remove(path);
  }

  TEST(Scene, ShouldRoundtripChildrenViaSaveLoad) {
    QTemporaryFile temp;
    ASSERT_TRUE(temp.open());
    auto path = temp.fileName();
    temp.close();

    Scene original;
    auto* camera = new PinholeCamera;
    original.addChild(camera);
    ASSERT_TRUE(original.save(path));

    Scene decoded;
    ASSERT_TRUE(decoded.load(path));
    EXPECT_NE(nullptr, decoded.activeCamera());
    EXPECT_NE(nullptr, qobject_cast<PinholeCamera*>(decoded.activeCamera()));

    QFile::remove(path);
  }

  TEST(Scene, ShouldProduceRaytracerSceneWithMatchingAmbient) {
    Scene scene;
    scene.setAmbient(Colord(0.7, 0.8, 0.9));
    auto rt = scene.toRaytracerScene();
    EXPECT_EQ(Colord(0.7, 0.8, 0.9), rt->ambient());
  }

  TEST(Scene, ShouldProduceRaytracerSceneWithMatchingBackground) {
    Scene scene;
    scene.setBackground(Colord(0.1, 0.2, 0.3));
    auto rt = scene.toRaytracerScene();
    EXPECT_EQ(Colord(0.1, 0.2, 0.3), rt->background());
  }

  TEST(Scene, ShouldProduceEmptyRaytracerSceneWithNoChildren) {
    Scene scene;
    auto rt = scene.toRaytracerScene();
    ASSERT_NE(nullptr, rt);
    EXPECT_EQ(0u, rt->lights().size());
  }
}
