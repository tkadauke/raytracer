#!/usr/bin/env python3
"""Stand-in for the `claude` CLI's `-p --output-format stream-json` mode.

Used only by ClaudeCliSession/ChatThread unit tests, per the chat-dock
issue's "automate what's mockable without a live CLI... with fakes/fixtures"
instruction — a live, already-authenticated `claude` login is never
available in CI. Emits the same event shapes ClaudeCliSession parses:
a `system` init event carrying `session_id`, one `assistant` message with a
text block and a `tool_use` block, one `user` message echoing back a
`tool_result`, and a final `result` summary.

Two message-text sentinels drive alternate paths so tests can exercise
ClaudeCliSession's failure handling without a special CLI flag:
- a message containing "TRIGGER_CRASH" exits nonzero before emitting anything
  (simulates the process dying before any stream-json framing).
- a message containing "TRIGGER_FAILURE" emits a `result` event with
  `is_error: true` and exits nonzero.
"""
import json
import sys
import uuid


def emit(event):
    sys.stdout.write(json.dumps(event) + "\n")
    sys.stdout.flush()


def value_after(args, flag):
    if flag not in args:
        return None
    index = args.index(flag)
    return args[index + 1] if index + 1 < len(args) else None


def main():
    args = sys.argv[1:]
    message = value_after(args, "-p") or ""
    resume = value_after(args, "--resume")
    session_id = resume or ("session-" + uuid.uuid4().hex[:8])

    if "TRIGGER_CRASH" in message:
        sys.exit(2)

    emit({"type": "system", "subtype": "init", "session_id": session_id})
    emit({
        "type": "assistant",
        "session_id": session_id,
        "message": {
            "role": "assistant",
            "content": [
                {"type": "text", "text": "Echo: " + message},
                {"type": "tool_use", "id": "toolu_1", "name": "query_scene", "input": {}},
            ],
        },
    })
    emit({
        "type": "user",
        "session_id": session_id,
        "message": {
            "role": "user",
            "content": [
                {
                    "type": "tool_result",
                    "tool_use_id": "toolu_1",
                    "content": "{\"elements\": []}",
                    "is_error": False,
                }
            ],
        },
    })

    if "TRIGGER_FAILURE" in message:
        emit({
            "type": "result",
            "subtype": "error_during_execution",
            "session_id": session_id,
            "is_error": True,
            "result": "simulated failure",
        })
        sys.exit(1)

    emit({
        "type": "result",
        "subtype": "success",
        "session_id": session_id,
        "is_error": False,
        "result": "Echo: " + message,
    })


if __name__ == "__main__":
    main()
