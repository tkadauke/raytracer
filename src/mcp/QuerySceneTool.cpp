#include "mcp/QuerySceneTool.h"

#include "world/objects/Scene.h"

namespace mcp {
  QJsonObject querySceneToJson(Scene& scene) {
    QJsonObject json;
    scene.write(json);
    return json;
  }
}
