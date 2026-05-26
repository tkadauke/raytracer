#ifndef GUI_TEST_HELPER_H
#define GUI_TEST_HELPER_H

#include <QApplication>
#include <QEvent>
#include <QEventLoop>

namespace testing {
  class GuiTest : public ::testing::Test {
  public:
    inline virtual void SetUp() {
      if (!QApplication::instance()) {
        static int argc = 1;
        static char applicationName[] = "unit_tests";
        static char* argv[] = {applicationName, nullptr};
        s_application = new QApplication(argc, argv);
      }
    }

    inline virtual void TearDown() {
      QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
      QApplication::processEvents(QEventLoop::AllEvents);
    }

  private:
    inline static QApplication* s_application = nullptr;
  };
}

#endif
