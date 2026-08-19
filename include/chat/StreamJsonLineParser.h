#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QVector>

namespace chat {

  /**
    * Incremental line-buffering parser for `claude --output-format
    * stream-json` output: one JSON object per `\n`-terminated line, arriving
    * in arbitrary-sized chunks off a `QProcess`'s stdout.
    *
    * Kept free of QObject/QProcess entirely so the framing logic — the part
    * most worth pinning with tests, per the "subprocess I/O framing" note in
    * the chat-dock issue — is testable with plain byte-array fixtures and no
    * event loop or live subprocess.
    */
  class StreamJsonLineParser {
  public:
    /**
      * Appends @p chunk to the internal buffer and returns every complete
      * (`\n`-terminated) line parsed as a JSON object since the last call.
      * A trailing partial line is held back until a future feed() completes
      * it. Blank lines and lines that don't parse as a JSON *object* (stray
      * CLI banner text, malformed JSON, a bare JSON array/scalar) are
      * silently skipped rather than surfaced as errors — the stream-json
      * contract is one object per line, and tolerating the odd non-JSON
      * line keeps a single glitch from wedging the whole session.
      */
    QVector<QJsonObject> feed(const QByteArray& chunk);

    /**
      * Flushes and parses a final unterminated line left in the buffer, for
      * when the process exits without a trailing newline. Returns an empty
      * object list if the buffer is empty or doesn't parse.
      */
    QVector<QJsonObject> flush();

  private:
    QVector<QJsonObject> consumeCompleteLines();

    QByteArray m_buffer;
  };

}
