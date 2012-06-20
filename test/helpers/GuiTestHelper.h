#ifndef GUI_TEST_HELPER
#define GUI_TEST_HELPER

#include <QApplication>

namespace testing {
  class GuiTest : public ::testing::Test {
  public:
    virtual void SetUp() {
      int argc = 0;
      char** argv = 0;
      m_application = new QApplication(argc, argv);
    }
    
    virtual void TearDown() {
      delete m_application;
    }
    
  private:
    QApplication* m_application;
  };
}

#endif
