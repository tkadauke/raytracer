#pragma once

#include <QJsonObject>

class Scene;

namespace mcp {
  /**
    * Builds a read-only JSON dump of @p scene's element graph (ids, types,
    * names, key parameters, and hierarchy) for the `query_scene` MCP tool.
    *
    * Reuses `Scene::write()` / `Element::write()` rather than inventing a
    * parallel schema, so the dump always matches the native scene JSON
    * format.
    */
  QJsonObject querySceneToJson(Scene& scene);
}
