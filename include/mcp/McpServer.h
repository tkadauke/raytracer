#pragma once

#include <QHostAddress>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <functional>
#include <memory>

class Scene;

namespace mcp {

  /**
    * Builds the `tools/call` result object (`content: [{type:"text",...}]`,
    * `isError`) for a plain-text tool response. Shared by the built-in
    * `query_scene` tool and every handler registered via
    * `McpServer::registerTool()`, so every tool's success/error envelope
    * looks the same on the wire.
    */
  QJsonObject toolTextResult(const QString& text, bool isError = false);

  /**
    * Embedded MCP (Model Context Protocol) server for the Modeler.
    *
    * Implements just enough of the legacy HTTP+SSE transport for the
    * `claude` CLI's `--mcp-config` (HTTP/SSE server type) to drive a live
    * Modeler instance:
    *
    * - `GET /sse` opens the server-to-client event stream and immediately
    *   pushes an `event: endpoint` event naming the per-session `POST`
    *   target (`/message?sessionId=...`).
    * - `POST /message?sessionId=...` submits a single JSON-RPC request; the
    *   HTTP response to the POST is just a `202 Accepted`, and the actual
    *   JSON-RPC response is pushed as an `event: message` event on the
    *   matching SSE connection.
    *
    * Only `initialize`, `tools/list`, and `tools/call` are implemented;
    * `tools/call` currently supports the single read-only `query_scene`
    * tool. The server binds loopback only (`127.0.0.1`) on an ephemeral
    * port, never `0.0.0.0`, and every request must present the per-session
    * bearer token issued by start().
    */
  class McpServer : public QObject {
    Q_OBJECT

  public:
    using SceneProvider = std::function<Scene*()>;

    /**
      * Describes one `tools/list` entry for a tool registered via
      * registerTool().
      */
    struct ToolDescriptor {
      QString name;
      QString description;
      QJsonObject inputSchema;
    };

    /**
      * Handles one `tools/call` invocation for a registered tool. Receives
      * the call's `arguments` object and returns the full `tools/call`
      * result object (`content`/`isError`) — see toolTextResult().
      */
    using ToolHandler = std::function<QJsonObject(const QJsonObject& arguments)>;

    /**
      * @p sceneProvider is invoked fresh for every `query_scene` tool call,
      * so it should return whatever scene is currently live (the pointer
      * behind it may be replaced across File > New / Open) rather than a
      * scene captured at construction time.
      */
    explicit McpServer(SceneProvider sceneProvider, QObject* parent = nullptr);
    ~McpServer() override;

    /**
      * Starts listening on an ephemeral loopback port and (re)generates the
      * per-session auth token.
      *
      * @returns false if the listening socket could not be opened.
      */
    bool start();

    /**
      * Stops listening and drops any open SSE connections.
      */
    void stop();

    [[nodiscard]] bool isRunning() const;

    /**
      * @returns the bound loopback port, or 0 when not running.
      */
    [[nodiscard]] quint16 port() const;

    /**
      * @returns the bound address (always `QHostAddress::LocalHost` while
      *   running), or `QHostAddress::Null` when not running. Exposed mainly
      *   so tests can assert the loopback-only binding directly against the
      *   listening socket rather than trusting a constructed URL string.
      */
    [[nodiscard]] QHostAddress address() const;

    /**
      * @returns the current per-session bearer token, or an empty string
      *   when not running.
      */
    [[nodiscard]] QString authToken() const;

    /**
      * @returns the `http://127.0.0.1:<port>/sse` URL clients should open
      *   the event stream against, or an empty string when not running.
      */
    [[nodiscard]] QString sseUrl() const;

    /**
      * Dispatches a single parsed JSON-RPC request to the appropriate MCP
      * method handler and returns the JSON-RPC response object.
      *
      * Exposed directly (independent of any socket) so the JSON-RPC framing
      * and dispatch logic — including the `query_scene` handler — can be
      * unit-tested without a real network connection.
      *
      * @returns an empty object for notifications (requests without an
      *   `id`), which must not be sent back to the client.
      */
    [[nodiscard]] QJsonObject handleJsonRpcRequest(const QJsonObject& request) const;

    /**
      * Adds (or, for a repeat name, replaces) a `tools/call`-dispatchable
      * tool beyond the built-in read-only `query_scene`. Mutating,
      * domain-specific tools (scene editing, etc.) register here instead of
      * being wired into McpServer's own JSON-RPC dispatch, so this class
      * stays a generic MCP transport rather than growing a case per tool.
      */
    void registerTool(const ToolDescriptor& descriptor, ToolHandler handler);

  private slots:
    void handleNewConnection();
    void handleSocketReadyRead();
    void handleSocketDisconnected();

  private:
    struct Private;
    std::unique_ptr<Private> p;
  };

}
