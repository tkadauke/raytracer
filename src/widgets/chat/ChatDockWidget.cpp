#include "widgets/chat/ChatDockWidget.h"

#include <QInputDialog>
#include <QLineEdit>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <memory>

#include "chat/ChatThread.h"
#include "chat/ChatThreadStore.h"
#include "widgets/chat/ChatThreadPanel.h"

struct ChatDockWidget::Private {
  QTabWidget* tabs = nullptr;
  QToolButton* newThreadButton = nullptr;
  QString mcpConfigPath;
  QString claudeExecutable{QStringLiteral("claude")};
  int threadCounter = 0;

  std::unique_ptr<chat::ChatThreadStore> store = std::make_unique<chat::ChatThreadStore>();
  QString sceneId;
  bool sceneIsPersisted = false;
};

ChatDockWidget::ChatDockWidget(QWidget* parent)
    : QWidget(parent),
      p(std::make_unique<Private>()) {
  p->tabs = new QTabWidget(this);
  p->tabs->setObjectName(QStringLiteral("chatThreadTabs"));
  p->tabs->setTabsClosable(true);
  connect(p->tabs, &QTabWidget::tabCloseRequested, this, &ChatDockWidget::closeTabRequested);
  connect(p->tabs->tabBar(), &QTabBar::tabBarDoubleClicked, this,
          &ChatDockWidget::tabDoubleClicked);

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

void ChatDockWidget::setThreadStoreBaseDirectory(const QString& baseDirectory) {
  p->store = std::make_unique<chat::ChatThreadStore>(baseDirectory);
}

void ChatDockWidget::setScene(const QString& sceneId, bool sceneIsPersisted) {
  if (!p->sceneId.isEmpty() && sceneId == p->sceneId) {
    // Same scene — most commonly "Save As" promoting a previously-unsaved
    // scene to persisted. Flush the currently open (until-now draft)
    // threads to disk instead of discarding and reloading them.
    p->sceneIsPersisted = sceneIsPersisted;
    if (p->sceneIsPersisted) {
      for (int i = 0; i < p->tabs->count(); ++i) {
        if (auto* panel = qobject_cast<ChatThreadPanel*>(p->tabs->widget(i)))
          persistThread(panel->thread());
      }
    }
    return;
  }

  p->sceneId = sceneId;
  p->sceneIsPersisted = sceneIsPersisted;
  p->threadCounter = 0;

  while (p->tabs->count() > 0) {
    QWidget* widget = p->tabs->widget(0);
    // A thread's `claude` turn can still be in flight when the scene
    // switches out from under it; disconnect before deleteLater() so a
    // late messageAppended()/busyChanged() can't persistThread() into the
    // *new* scene's directory once p->sceneId has moved on below.
    if (auto* panel = qobject_cast<ChatThreadPanel*>(widget)) {
      if (auto* thread = panel->thread())
        thread->disconnect(this);
    }
    p->tabs->removeTab(0);
    if (widget)
      widget->deleteLater();
  }

  std::vector<chat::ChatThreadRecord> records;
  if (p->sceneIsPersisted && p->store)
    records = p->store->loadThreads(sceneId);

  if (records.empty()) {
    addThread();
    return;
  }

  for (auto& record : records) {
    auto* thread = new chat::ChatThread(record.id, record.name, this);
    thread->restore(record.sessionId, std::move(record.messages));
    installThreadTab(thread);
  }
  p->tabs->setCurrentIndex(0);
}

void ChatDockWidget::installThreadTab(chat::ChatThread* thread) {
  auto* panel = new ChatThreadPanel(thread, p->tabs);
  // Tie the thread's lifetime to its panel rather than to this dock, so
  // closing a single tab (closeTabRequested()) cleans up both together.
  thread->setParent(panel);

  panel->setMcpConfigPath(p->mcpConfigPath);
  panel->setClaudeExecutable(p->claudeExecutable);

  connect(thread, &chat::ChatThread::messageAppended, this,
          [this, thread](int) { persistThread(thread); });
  connect(thread, &chat::ChatThread::busyChanged, this, [this, thread](bool busy) {
    if (!busy)
      persistThread(thread);
  });

  p->tabs->addTab(panel, thread->name());
}

void ChatDockWidget::persistThread(chat::ChatThread* thread) {
  if (!thread || !p->sceneIsPersisted || p->sceneId.isEmpty() || !p->store)
    return;

  chat::ChatThreadRecord record;
  record.id = thread->id();
  record.name = thread->name();
  record.sessionId = thread->sessionId();
  record.messages = thread->messages();
  p->store->saveThread(p->sceneId, record);
}

chat::ChatThread* ChatDockWidget::addThread(const QString& name) {
  ++p->threadCounter;
  const QString threadName = name.isEmpty() ? tr("Chat %1").arg(p->threadCounter) : name;

  auto* thread = new chat::ChatThread(threadName, this);
  installThreadTab(thread);
  persistThread(thread);

  p->tabs->setCurrentIndex(p->tabs->count() - 1);
  return thread;
}

void ChatDockWidget::renameThread(int index, const QString& name) {
  const QString trimmed = name.trimmed();
  if (trimmed.isEmpty())
    return;

  auto* panel = qobject_cast<ChatThreadPanel*>(p->tabs->widget(index));
  if (!panel || !panel->thread())
    return;

  panel->thread()->setName(trimmed);
  p->tabs->setTabText(index, trimmed);
  persistThread(panel->thread());
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
  if (auto* panel = qobject_cast<ChatThreadPanel*>(widget)) {
    if (auto* thread = panel->thread()) {
      if (p->sceneIsPersisted && p->store && !p->sceneId.isEmpty())
        p->store->deleteThread(p->sceneId, thread->id());
      thread->disconnect(this);
    }
  }

  p->tabs->removeTab(index);
  if (widget)
    widget->deleteLater();
}

void ChatDockWidget::tabDoubleClicked(int index) {
  auto* panel = qobject_cast<ChatThreadPanel*>(p->tabs->widget(index));
  if (!panel || !panel->thread())
    return;

  bool ok = false;
  const QString name = QInputDialog::getText(this, tr("Rename Chat"), tr("Name"),
                                              QLineEdit::Normal, panel->thread()->name(), &ok);
  if (ok)
    renameThread(index, name);
}
