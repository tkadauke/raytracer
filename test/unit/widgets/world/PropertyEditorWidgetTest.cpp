#include <gtest/gtest.h>

#include "widgets/world/PropertyEditorWidget.h"
#include "widgets/world/AbstractParameterWidget.h"
#include "world/objects/Scene.h"
#include "world/objects/PinholeCamera.h"
#include "core/math/Vector.h"
#include "core/math/Angle.h"
#include "core/Color.h"

#include "test/helpers/GuiTestHelper.h"

Q_DECLARE_METATYPE(Vector3d);
Q_DECLARE_METATYPE(Angled);
Q_DECLARE_METATYPE(Colord);

namespace PropertyEditorWidgetTest {
  struct MetaTypeRegistrar {
    MetaTypeRegistrar() {
      qRegisterMetaType<Vector3d>();
      qRegisterMetaType<Angled>();
      qRegisterMetaType<Colord>();
    }
  };
  static const MetaTypeRegistrar s_registrar;

  class PropertyEditorWidgetTest : public ::testing::GuiTest {};

  AbstractParameterWidget* parameterWidget(PropertyEditorWidget& editor, const QString& name) {
    const auto widgets = editor.findChildren<AbstractParameterWidget*>();
    for (auto* widget : widgets) {
      if (widget->parameterName() == name)
        return widget;
    }
    return nullptr;
  }

  TEST_F(PropertyEditorWidgetTest, ShouldInitializeWithRoot) {
    Scene root;
    PropertyEditorWidget editor(&root);
  }

  TEST_F(PropertyEditorWidgetTest, ShouldInitializeWithNullRoot) {
    // PropertyEditorWidget doesn't deref root in the ctor — it caches the
    // pointer for later setElement calls. nullptr is valid until setRoot
    // hands one in.
    PropertyEditorWidget editor(nullptr);
  }

  TEST_F(PropertyEditorWidgetTest, ShouldReturnCannedSizeHint) {
    // The widget's sizeHint is hard-coded to 256×100; verifying it pins
    // the layout decision in case a future change forgets to override.
    Scene root;
    PropertyEditorWidget editor(&root);
    EXPECT_EQ(QSize(256, 100), editor.sizeHint());
  }

  TEST_F(PropertyEditorWidgetTest, ShouldAcceptNullSetElement) {
    // setElement(nullptr) is the "deselect" case used when the user clicks
    // empty space in the tree view; clears the
    // parameter widgets without crashing.
    Scene root;
    PropertyEditorWidget editor(&root);
    editor.setElement(nullptr);
  }

  TEST_F(PropertyEditorWidgetTest, ShouldAcceptElementSelection) {
    // Setting an element with several Q_PROPERTYs (PinholeCamera has
    // distance, zoom, plus inherited position/target) populates the
    // editor's parameter widget list. Verified indirectly by checking
    // the ctor + setElement chain doesn't throw or crash.
    Scene root;
    auto* camera = new PinholeCamera;
    root.addChild(camera);

    PropertyEditorWidget editor(&root);
    editor.setElement(camera);
  }

  TEST_F(PropertyEditorWidgetTest, ShouldReplaceRootOnSetRoot) {
    Scene root1;
    PropertyEditorWidget editor(&root1);
    Scene root2;
    editor.setRoot(&root2);
    // setRoot also clears the current element selection, so a subsequent
    // setElement(nullptr) is the no-op deselect case.
    editor.setElement(nullptr);
  }

  TEST_F(PropertyEditorWidgetTest, ShouldCreateVectorParameterWidgets) {
    Scene root;
    auto* camera = new PinholeCamera;
    root.addChild(camera);

    PropertyEditorWidget editor(&root);
    editor.setElement(camera);

    auto* position = parameterWidget(editor, "position");
    auto* target = parameterWidget(editor, "target");
    ASSERT_NE(nullptr, position);
    ASSERT_NE(nullptr, target);
    EXPECT_TRUE(position->isEnabled());
    EXPECT_TRUE(target->isEnabled());
  }
}
