#include "widgets/chat/ChatDockWidget.h"

#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "chat/ChatThread.h"
#include "widgets/chat/ChatThreadPanel.h"

struct ChatDockWidget::Private {
  QTabWidget* tabs = nullptr;
  QToolButton* newThreadButton = nullptr;
  QString mcpConfigPath;
  QString claudeExecutable{QStringLiteral("claude")};
  int threadCounter = 0;
};

ChatDockWidget::ChatDockWidget(QWidget* parent)
    : QWidget(parent),
      p(std::make_unique<Private>()) {
  p->tabs = new QTabWidget(this);
  p->tabs->setObjectName(QStringLiteral("chatThreadTabs"));
  p->tabs->setTabsClosable(true);
  connect(p->tabs, &QTabWidget::tabCloseRequested, this, &ChatDockWidget::closeTabRequested);

  p->newThreadButton = new QToolButton(this);
  p->newThreadButton->setObjectName(QStringLiteral("newChatThreadButton"));
  p->newThreadButton->setText(QStringLiteral("+"));
  p->newThreadButton->setToolTip(tr("New Chat"));
  connect(p->newThreadButton, &QToolButton::clicked, this, &ChatDockWidget::newThreadButtonClicked);
  p->tabs->setCornerWidget(p->newThreadButton, Qt::TopRightCorner);

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(p->tabs);
  setLayout(layout);

  addThread();
}

ChatDockWidget::~ChatDockWidget() = default;

void ChatDockWidget::setMcpConfigPath(const QString& path) {
  p->mcpConfigPath = path;
  for (int i = 0; i < p->tabs->count(); ++i) {
    if (auto* panel = qobject_cast<ChatThreadPanel*>(p->tabs->widget(i)))
      panel->setMcpConfigPath(path);
  }
}

void ChatDockWidget::setClaudeExecutable(const QString& executable) {
  p->claudeExecutable = executable;
  for (int i = 0; i < p->tabs->count(); ++i) {
    if (auto* panel = qobject_cast<ChatThreadPanel*>(p->tabs->widget(i)))
      panel->setClaudeExecutable(executable);
  }
}

chat::ChatThread* ChatDockWidget::addThread(const QString& name) {
  ++p->threadCounter;
  const QString threadName = name.isEmpty() ? tr("Chat %1").arg(p->threadCounter) : name;

  auto* thread = new chat::ChatThread(threadName, this);
  auto* panel = new ChatThreadPanel(thread, p->tabs);
  // Tie the thread's lifetime to its panel rather than to this dock, so
  // closing a single tab (closeTabRequested()) cleans up both together.
  thread->setParent(panel);

  panel->setMcpConfigPath(p->mcpConfigPath);
  panel->setClaudeExecutable(p->claudeExecutable);

  p->tabs->addTab(panel, threadName);
  p->tabs->setCurrentWidget(panel);

  return thread;
}

int ChatDockWidget::threadCount() const {
  return p->tabs->count();
}

chat::ChatThread* ChatDockWidget::threadAt(int index) const {
  if (auto* panel = qobject_cast<ChatThreadPanel*>(p->tabs->widget(index)))
    return panel->thread();
  return nullptr;
}

void ChatDockWidget::newThreadButtonClicked() {
  addThread();
}

void ChatDockWidget::closeTabRequested(int index) {
  QWidget* widget = p->tabs->widget(index);
  p->tabs->removeTab(index);
  if (widget)
    widget->deleteLater();
}
