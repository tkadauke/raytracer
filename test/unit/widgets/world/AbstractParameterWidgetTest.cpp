#include <gtest/gtest.h>

#include "widgets/world/AbstractParameterWidget.h"
#include "world/objects/Element.h"

#include "test/helpers/GuiTestHelper.h"
#include "test/helpers/Signal.h"
#include "test/helpers/Slot.h"

namespace AbstractParameterWidgetTest {
  // Concrete stand-in for the abstract base — value/setValue are required
  // overrides, plus a hook to simulate a parameterChanged() signal arrival
  // from a Qt input control. Kept inside the test TU so we can reach
  // parameterChanged() (a protected slot) by calling it ourselves.
  class TestParameterWidget : public AbstractParameterWidget {
    Q_OBJECT
  public:
    explicit TestParameterWidget(QWidget* parent = nullptr)
        : AbstractParameterWidget(parent),
          m_value() {
    }
    const QVariant value() const override {
      return m_value;
    }
    void setValue(const QVariant& v) override {
      m_value = v;
    }

    // Public hook to fire parameterChanged() from outside; the slot is
    // protected on the base, but visible to the subclass.
    void simulateInput() {
      parameterChanged();
    }

  private:
    QVariant m_value;
  };

  class AbstractParameterWidgetTest : public ::testing::GuiTest {};

  TEST_F(AbstractParameterWidgetTest, ShouldInitialize) {
    TestParameterWidget widget;
  }

  TEST_F(AbstractParameterWidgetTest, ShouldDefaultToEmptyParameterName) {
    TestParameterWidget widget;
    EXPECT_TRUE(widget.parameterName().isEmpty());
  }

  TEST_F(AbstractParameterWidgetTest, ShouldSetAndGetParameterName) {
    TestParameterWidget widget;
    widget.setParameterName("size");
    EXPECT_EQ(QString("size"), widget.parameterName());
  }

  TEST_F(AbstractParameterWidgetTest, ShouldEmitChangedOnParameterChanged) {
    TestParameterWidget widget;
    widget.setParameterName("brightness");
    widget.setValue(QVariant::fromValue(0.75));

    Slot slot;
    QObject::connect(&widget, SIGNAL(changed(const QString&, const QVariant&)), &slot,
                     SLOT(receive()));
    widget.simulateInput();

    EXPECT_TRUE(slot.called());
  }

  TEST_F(AbstractParameterWidgetTest, ShouldAcceptNullElement) {
    // setElement(nullptr) is a documented no-op — the widget caches the
    // pointer for downstream property hand-off but doesn't deref it.
    TestParameterWidget widget;
    widget.setElement(nullptr);
  }
}

#include "AbstractParameterWidgetTest.moc"
