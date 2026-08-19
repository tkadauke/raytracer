#include "widgets/chat/ChatThreadPanel.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include "chat/ChatMessage.h"
#include "chat/ChatMessageFormatter.h"
#include "chat/ChatThread.h"

struct ChatThreadPanel::Private {
  chat::ChatThread* thread = nullptr;
  QTextEdit* transcript = nullptr;
  QLineEdit* input = nullptr;
  QPushButton* sendButton = nullptr;
  QString mcpConfigPath;
  QString claudeExecutable{QStringLiteral("claude")};
};

ChatThreadPanel::ChatThreadPanel(chat::ChatThread* thread, QWidget* parent)
    : QWidget(parent),
      p(std::make_unique<Private>()) {
  p->thread = thread;

  p->transcript = new QTextEdit(this);
  p->transcript->setReadOnly(true);
  p->transcript->setObjectName(QStringLiteral("chatTranscript"));

  p->input = new QLineEdit(this);
  p->input->setObjectName(QStringLiteral("chatInput"));
  p->input->setPlaceholderText(tr("Message Claude..."));

  p->sendButton = new QPushButton(tr("Send"), this);
  p->sendButton->setObjectName(QStringLiteral("chatSendButton"));

  auto* inputLayout = new QHBoxLayout;
  inputLayout->addWidget(p->input, 1);
  inputLayout->addWidget(p->sendButton);

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(p->transcript, 1);
  layout->addLayout(inputLayout);
  setLayout(layout);

  connect(p->input, &QLineEdit::returnPressed, this, &ChatThreadPanel::sendClicked);
  connect(p->sendButton, &QPushButton::clicked, this, &ChatThreadPanel::sendClicked);
  connect(p->thread, &chat::ChatThread::messageAppended, this,
          &ChatThreadPanel::handleMessageAppended);
  connect(p->thread, &chat::ChatThread::busyChanged, this, &ChatThreadPanel::handleBusyChanged);
}

ChatThreadPanel::~ChatThreadPanel() = default;

chat::ChatThread* ChatThreadPanel::thread() const {
  return p->thread;
}

void ChatThreadPanel::setMcpConfigPath(const QString& path) {
  p->mcpConfigPath = path;
}

void ChatThreadPanel::setClaudeExecutable(const QString& executable) {
  p->claudeExecutable = executable;
}

void ChatThreadPanel::sendClicked() {
  const QString text = p->input->text().trimmed();
  if (text.isEmpty() || p->thread->isBusy())
    return;

  p->thread->sendMessage(text, p->mcpConfigPath, p->claudeExecutable);
  p->input->clear();
}

void ChatThreadPanel::handleMessageAppended(int index) {
  const auto& messages = p->thread->messages();
  if (index < 0 || static_cast<size_t>(index) >= messages.size())
    return;

  p->transcript->append(chat::formatChatMessageHtml(messages[static_cast<size_t>(index)]));
}

void ChatThreadPanel::handleBusyChanged(bool busy) {
  p->input->setEnabled(!busy);
  p->sendButton->setEnabled(!busy);
  p->sendButton->setText(busy ? tr("Sending...") : tr("Send"));
}
