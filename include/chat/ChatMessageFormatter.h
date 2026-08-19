#pragma once

#include <QString>

namespace chat {

  struct ChatMessage;

  /**
    * Renders a single `ChatMessage` as an HTML fragment suitable for
    * `QTextEdit::append()`/`QTextBrowser::append()`. Kept as a pure
    * function (no widget dependency) so the rendering rules — what a tool
    * call vs. a tool error vs. plain assistant text looks like — are
    * unit-testable without a `QApplication` or a live `ChatThread`.
    *
    * All message text is HTML-escaped; only the formatting markup below is
    * trusted.
    */
  QString formatChatMessageHtml(const ChatMessage& message);

}
