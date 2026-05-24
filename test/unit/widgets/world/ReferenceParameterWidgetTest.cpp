#include <gtest/gtest.h>

#include "widgets/world/ReferenceParameterWidget.h"
#include "world/objects/Scene.h"
#include "world/objects/MatteMaterial.h"
#include "core/Exception.h"

#include "test/helpers/GuiTestHelper.h"

Q_DECLARE_METATYPE(Material*);

namespace ReferenceParameterWidgetTest {
  struct MetaTypeRegistrar {
    MetaTypeRegistrar() {
      qRegisterMetaType<Material*>();
    }
  };
  static const MetaTypeRegistrar s_registrar;

  class ReferenceParameterWidgetTest : public ::testing::GuiTest {};

  TEST_F(ReferenceParameterWidgetTest, ShouldInitializeWithMaterialBaseClass) {
    Scene root;
    ReferenceParameterWidget widget("Material", &root);
  }

  TEST_F(ReferenceParameterWidgetTest, ShouldDefaultToNoSelection) {
    // The widget always inserts a "<No Selection>" entry as index 0 so the
    // user can clear a reference; the default current selection is that
    // sentinel, which round-trips through value() as a null Material*.
    Scene root;
    ReferenceParameterWidget widget("Material", &root);
    auto v = widget.value();
    EXPECT_EQ(nullptr, v.value<Material*>());
  }

  TEST_F(ReferenceParameterWidgetTest, ShouldRoundtripMaterialReference) {
    Scene root;
    auto* material = new MatteMaterial;
    material->setName("metal");
    root.addChild(material);

    ReferenceParameterWidget widget("Material", &root);
    widget.setValue(QVariant::fromValue<Material*>(material));
    EXPECT_EQ(material, widget.value().value<Material*>());
  }

  TEST_F(ReferenceParameterWidgetTest, ShouldThrowOnUnknownBaseClassWhenPopulating) {
    // makeVariant only knows about "Material" and "Texture"; passing a
    // root whose tree contains an element matching an unknown base class
    // would throw at construction time during fillComboBox. Use Element
    // (matches anything via QObject::inherits("Element")) to trigger the
    // throw branch with the unknown base name.
    Scene root;
    // The project's Exception type doesn't derive from std::exception —
    // it's a standalone class with its own backtrace machinery. Catch by
    // its actual type.
    EXPECT_THROW(ReferenceParameterWidget widget("Bogus", &root), Exception);
  }
}
