#pragma once

namespace mcp {

  class McpServer;
  class SceneEditor;

  /**
    * Registers the mutating scene-editing MCP tools (roadmap §4.6.i, v1 tool
    * surface) on @p server: `add_primitive`, `transform`, `apply_material`,
    * `select`, `delete`, `csg_union`, `csg_intersect`, `csg_difference`, and
    * `set_camera`. Each tool's handler parses its `tools/call` `arguments`
    * object and forwards to the matching mcp::SceneEditor method, so the
    * actual mutation logic lives in one place shared with SceneEditor's own
    * unit tests.
    *
    * Deliberately not included here (see the roadmap job body): `import_file`,
    * `set_environment`, `run_script`, `get_/set_modifier_stack`.
    */
  void registerSceneEditingTools(McpServer& server, SceneEditor& editor);

}
