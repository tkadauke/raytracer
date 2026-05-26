#include <gtest/gtest.h>

#include "widgets/world/PropertyEditorWidget.h"
#include "widgets/world/AbstractParameterWidget.h"
#include "widgets/world/ChoiceParameterWidget.h"
#include "world/objects/Scene.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/Group.h"
#include "world/objects/RenderIntentElement.h"
#include "core/math/Vector.h"
#include "core/math/Angle.h"
#include "core/Color.h"

#include "test/helpers/GuiTestHelper.h"

#include <QLabel>
#include <QComboBox>
#include <QCoreApplication>
#include <QGroupBox>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTreeWidget>

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

  RenderIntentElement* renderSettingsElement(Scene& scene) {
    for (Element* child : scene.childElements()) {
      if (auto* settings = qobject_cast<RenderIntentElement*>(child))
        return settings;
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
    // The editor should stay compact enough for a docked sidebar.
    Scene root;
    PropertyEditorWidget editor(&root);
    EXPECT_EQ(QSize(180, 100), editor.sizeHint());
  }

  TEST_F(PropertyEditorWidgetTest, ShouldUseScrollableContentArea) {
    Scene root;
    auto* camera = new PinholeCamera;
    root.addChild(camera);

    PropertyEditorWidget editor(&root);
    editor.setElement(camera);

    auto* scrollArea = editor.findChild<QScrollArea*>("propertyEditorScrollArea");
    ASSERT_NE(nullptr, scrollArea);
    EXPECT_TRUE(scrollArea->widgetResizable());
    EXPECT_EQ(Qt::ScrollBarAlwaysOff, scrollArea->horizontalScrollBarPolicy());
    EXPECT_EQ(Qt::ScrollBarAsNeeded, scrollArea->verticalScrollBarPolicy());
    ASSERT_NE(nullptr, scrollArea->widget());
    EXPECT_EQ(QStringLiteral("propertyEditorContent"), scrollArea->widget()->objectName());
  }

  TEST_F(PropertyEditorWidgetTest, ShouldAvoidEagerVerticalExpansion) {
    Scene root;
    PropertyEditorWidget editor(&root);
    EXPECT_EQ(QSizePolicy::Minimum, editor.sizePolicy().verticalPolicy());
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

  TEST_F(PropertyEditorWidgetTest, ShouldHumanizePropertyNames) {
    Scene root;
    auto* camera = new PinholeCamera;
    root.addChild(camera);

    PropertyEditorWidget editor(&root);
    editor.setElement(camera);

    auto* position = parameterWidget(editor, "position");
    ASSERT_NE(nullptr, position);
    const auto labels = position->findChildren<QLabel*>();
    bool foundHumanName = false;
    for (const auto* label : labels) {
      foundHumanName = foundHumanName || label->text() == QStringLiteral("Position");
    }
    EXPECT_TRUE(foundHumanName);
  }

  TEST_F(PropertyEditorWidgetTest, ShouldUseChoicesForRenderSettingsEnums) {
    Scene root;
    auto* settings = renderSettingsElement(root);
    ASSERT_NE(nullptr, settings);

    PropertyEditorWidget editor(&root);
    editor.setElement(settings);

    auto* defaultEngine = parameterWidget(editor, "defaultEngine");
    ASSERT_NE(nullptr, qobject_cast<ChoiceParameterWidget*>(defaultEngine));
    auto* comboBox = defaultEngine->findChild<QComboBox*>("choiceComboBox");
    ASSERT_NE(nullptr, comboBox);
    EXPECT_GE(comboBox->count(), 3);
    EXPECT_NE(-1, comboBox->findData(QString("rasterizer")));
  }

  TEST_F(PropertyEditorWidgetTest, ShouldUseChoicesForDiscreteIntegerRenderSettings) {
    Scene root;
    auto* settings = renderSettingsElement(root);
    ASSERT_NE(nullptr, settings);
    settings->setDefaultEngine("rasterizer");

    PropertyEditorWidget editor(&root);
    editor.setElement(settings);

    auto* msaaSamples = parameterWidget(editor, "rasterizerMSAASamples");
    ASSERT_NE(nullptr, qobject_cast<ChoiceParameterWidget*>(msaaSamples));
    auto* comboBox = msaaSamples->findChild<QComboBox*>("choiceComboBox");
    ASSERT_NE(nullptr, comboBox);
    ASSERT_EQ(4, comboBox->count());
    EXPECT_EQ(1, comboBox->itemData(0).toInt());
    EXPECT_EQ(8, comboBox->itemData(3).toInt());
  }

  TEST_F(PropertyEditorWidgetTest, ShouldApplyNumericRangesFromSelectedElement) {
    Scene root;
    auto* settings = renderSettingsElement(root);
    ASSERT_NE(nullptr, settings);

    PropertyEditorWidget editor(&root);
    editor.setElement(settings);

    auto* samples = parameterWidget(editor, "raytracerSamplesPerPixel");
    ASSERT_NE(nullptr, samples);
    auto* spinBox = samples->findChild<QSpinBox*>("intEdit");
    ASSERT_NE(nullptr, spinBox);
    EXPECT_EQ(1, spinBox->minimum());
  }

  TEST_F(PropertyEditorWidgetTest, ShouldGroupAndFilterRenderSettingsBySelectedEngine) {
    Scene root;
    auto* settings = renderSettingsElement(root);
    ASSERT_NE(nullptr, settings);

    PropertyEditorWidget editor(&root);
    editor.setElement(settings);

    EXPECT_NE(nullptr, editor.findChild<QGroupBox*>("propertyGroupGeneral"));
    EXPECT_NE(nullptr, editor.findChild<QGroupBox*>("propertyGroupRaytracer"));
    EXPECT_EQ(nullptr, parameterWidget(editor, "rasterizerLod"));

    auto* defaultEngine = parameterWidget(editor, "defaultEngine");
    ASSERT_NE(nullptr, defaultEngine);
    auto* comboBox = defaultEngine->findChild<QComboBox*>("choiceComboBox");
    ASSERT_NE(nullptr, comboBox);
    comboBox->setCurrentIndex(comboBox->findData(QString("rasterizer")));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    EXPECT_EQ(nullptr, parameterWidget(editor, "raytracerSampler"));
    EXPECT_NE(nullptr, parameterWidget(editor, "rasterizerLod"));
    EXPECT_NE(nullptr, editor.findChild<QGroupBox*>("propertyGroupRasterizer"));
  }

  TEST_F(PropertyEditorWidgetTest, ShouldKeepRenderSettingsMinimumWidthCompact) {
    Scene root;
    auto* settings = renderSettingsElement(root);
    ASSERT_NE(nullptr, settings);

    PropertyEditorWidget editor(&root);
    editor.setElement(settings);

    EXPECT_LE(editor.minimumSizeHint().width(), 220);
  }

  TEST_F(PropertyEditorWidgetTest, ShouldShowReadOnlyProperties) {
    Scene root;
    PropertyEditorWidget editor(&root);
    editor.setReadOnlyProperties("Render graph pass", {{"Pass", "tonemap"}, {"Trace", "done"}});

    auto* title = editor.findChild<QLabel*>("propertyEditorReadOnlyTitle");
    auto* rows = editor.findChild<QTreeWidget*>("propertyEditorReadOnlyProperties");
    ASSERT_NE(nullptr, title);
    ASSERT_NE(nullptr, rows);
    EXPECT_EQ(QString("Render graph pass"), title->text());
    ASSERT_EQ(2, rows->topLevelItemCount());
    EXPECT_EQ(QString("Pass"), rows->topLevelItem(0)->text(0));
    EXPECT_EQ(QString("tonemap"), rows->topLevelItem(0)->text(1));
  }

  TEST_F(PropertyEditorWidgetTest, ShouldShowGroupPropertiesAndMetadata) {
    Scene root;
    auto* group = new Group;
    group->setMetadataValue("source", QString("import"));
    root.addChild(group);

    PropertyEditorWidget editor(&root);
    editor.setElement(group);

    EXPECT_NE(nullptr, parameterWidget(editor, "name"));
    EXPECT_NE(nullptr, parameterWidget(editor, "position"));
    EXPECT_NE(nullptr, parameterWidget(editor, "visible"));

    auto* metadata = editor.findChild<QTreeWidget*>("propertyEditorGroupMetadata");
    ASSERT_NE(nullptr, metadata);
    ASSERT_EQ(1, metadata->topLevelItemCount());
    EXPECT_EQ(QString("source"), metadata->topLevelItem(0)->text(0));
    EXPECT_EQ(QString("\"import\""), metadata->topLevelItem(0)->text(1));
  }
}
