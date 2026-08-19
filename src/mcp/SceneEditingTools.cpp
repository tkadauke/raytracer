#include "mcp/SceneEditingTools.h"

#include "mcp/McpServer.h"
#include "mcp/SceneEditor.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <utility>

namespace mcp {

  namespace {
    QJsonObject vector3Schema(const QString& description) {
      QJsonObject items;
      items[QStringLiteral("type")] = QStringLiteral("number");

      QJsonObject schema;
      schema[QStringLiteral("type")] = QStringLiteral("array");
      schema[QStringLiteral("items")] = items;
      schema[QStringLiteral("minItems")] = 3;
      schema[QStringLiteral("maxItems")] = 3;
      schema[QStringLiteral("description")] = description;
      return schema;
    }

    QJsonObject stringSchema(const QString& description) {
      QJsonObject schema;
      schema[QStringLiteral("type")] = QStringLiteral("string");
      schema[QStringLiteral("description")] = description;
      return schema;
    }

    QJsonObject objectSchema(QJsonObject properties, const QJsonArray& required) {
      QJsonObject schema;
      schema[QStringLiteral("type")] = QStringLiteral("object");
      schema[QStringLiteral("properties")] = std::move(properties);
      if (!required.isEmpty())
        schema[QStringLiteral("required")] = required;
      return schema;
    }

    QJsonObject resultToJson(const EditResult& result) {
      QJsonObject data;
      data[QStringLiteral("ok")] = result.ok;
      if (!result.id.isEmpty())
        data[QStringLiteral("id")] = result.id;
      if (!result.message.isEmpty())
        data[result.ok ? QStringLiteral("note") : QStringLiteral("error")] = result.message;

      return toolTextResult(
        QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact)), !result.ok);
    }

    McpServer::ToolDescriptor addPrimitiveDescriptor() {
      QJsonObject properties;
      properties[QStringLiteral("type")] =
        stringSchema(QStringLiteral("Box, Sphere, Cylinder, Ring, or Torus."));
      properties[QStringLiteral("position")] =
        vector3Schema(QStringLiteral("World-space [x, y, z] position."));
      properties[QStringLiteral("params")] =
        objectSchema(QJsonObject(), {}); // arbitrary extra Q_PROPERTY overrides
      return McpServer::ToolDescriptor{
        QStringLiteral("add_primitive"),
        QStringLiteral("Adds a Box/Sphere/Cylinder/Ring/Torus primitive to the scene."),
        objectSchema(properties, {QStringLiteral("type")})};
    }

    McpServer::ToolDescriptor transformDescriptor() {
      QJsonObject properties;
      properties[QStringLiteral("id")] = stringSchema(QStringLiteral("Target element id."));
      properties[QStringLiteral("translate")] =
        vector3Schema(QStringLiteral("Absolute [x, y, z] position."));
      properties[QStringLiteral("rotate")] =
        vector3Schema(QStringLiteral("Absolute Euler angles [x, y, z] in radians."));
      properties[QStringLiteral("scale")] =
        vector3Schema(QStringLiteral("Absolute [x, y, z] scale factors."));
      return McpServer::ToolDescriptor{
        QStringLiteral("transform"),
        QStringLiteral(
          "Sets position/rotation/scale on a Transformable element. Provide any subset of "
          "translate/rotate/scale."),
        objectSchema(properties, {QStringLiteral("id")})};
    }

    McpServer::ToolDescriptor applyMaterialDescriptor() {
      QJsonObject properties;
      properties[QStringLiteral("id")] = stringSchema(QStringLiteral("Target surface id."));
      QJsonObject materialSchema;
      materialSchema[QStringLiteral("description")] =
        QStringLiteral("Either an existing material element id, or an inline "
                       "{\"type\": \"MatteMaterial\"|\"PhongMaterial\"|\"TransparentMaterial\", "
                       "\"params\": {...}} object.");
      properties[QStringLiteral("material")] = materialSchema;
      return McpServer::ToolDescriptor{
        QStringLiteral("apply_material"),
        QStringLiteral("Attaches a material (by id, or created inline) to a surface."),
        objectSchema(properties, {QStringLiteral("id"), QStringLiteral("material")})};
    }

    McpServer::ToolDescriptor selectDescriptor() {
      QJsonObject properties;
      properties[QStringLiteral("id")] =
        stringSchema(QStringLiteral("Element id to select, or empty to clear the selection."));
      return McpServer::ToolDescriptor{
        QStringLiteral("select"),
        QStringLiteral("Selects an element in the Elements dock, exactly as a tree-view click would."),
        objectSchema(properties, {QStringLiteral("id")})};
    }

    McpServer::ToolDescriptor deleteDescriptor() {
      QJsonObject properties;
      properties[QStringLiteral("id")] = stringSchema(QStringLiteral("Element id to delete."));
      return McpServer::ToolDescriptor{
        QStringLiteral("delete"),
        QStringLiteral("Deletes an element from the scene."),
        objectSchema(properties, {QStringLiteral("id")})};
    }

    McpServer::ToolDescriptor csgDescriptor(const QString& name, const QString& verb) {
      QJsonObject properties;
      properties[QStringLiteral("a")] = stringSchema(QStringLiteral("First operand surface id."));
      properties[QStringLiteral("b")] = stringSchema(QStringLiteral("Second operand surface id."));
      return McpServer::ToolDescriptor{
        name,
        QStringLiteral("Creates a CSG %1 of two surfaces and reparents them under it.").arg(verb),
        objectSchema(properties, {QStringLiteral("a"), QStringLiteral("b")})};
    }

    McpServer::ToolDescriptor setCameraDescriptor() {
      QJsonObject properties;
      properties[QStringLiteral("position")] =
        vector3Schema(QStringLiteral("Absolute [x, y, z] camera position."));
      properties[QStringLiteral("target")] =
        vector3Schema(QStringLiteral("Absolute [x, y, z] look-at target."));
      QJsonObject fovSchema;
      fovSchema[QStringLiteral("type")] = QStringLiteral("number");
      fovSchema[QStringLiteral("description")] =
        QStringLiteral("Field of view in degrees. Only applied when the active camera type "
                       "exposes a single fieldOfView property.");
      properties[QStringLiteral("fov")] = fovSchema;
      return McpServer::ToolDescriptor{
        QStringLiteral("set_camera"),
        QStringLiteral("Sets position/target/fov on the scene's active camera."),
        objectSchema(properties, {})};
    }
  }

  void registerSceneEditingTools(McpServer& server, SceneEditor& editor) {
    server.registerTool(addPrimitiveDescriptor(), [&editor](const QJsonObject& args) {
      return resultToJson(editor.addPrimitive(args.value(QStringLiteral("type")).toString(),
                                              args.value(QStringLiteral("position")),
                                              args.value(QStringLiteral("params")).toObject()));
    });

    server.registerTool(transformDescriptor(), [&editor](const QJsonObject& args) {
      return resultToJson(editor.transform(
        args.value(QStringLiteral("id")).toString(), args.value(QStringLiteral("translate")),
        args.value(QStringLiteral("rotate")), args.value(QStringLiteral("scale"))));
    });

    server.registerTool(applyMaterialDescriptor(), [&editor](const QJsonObject& args) {
      return resultToJson(editor.applyMaterial(args.value(QStringLiteral("id")).toString(),
                                               args.value(QStringLiteral("material"))));
    });

    server.registerTool(selectDescriptor(), [&editor](const QJsonObject& args) {
      return resultToJson(editor.select(args.value(QStringLiteral("id")).toString()));
    });

    server.registerTool(deleteDescriptor(), [&editor](const QJsonObject& args) {
      return resultToJson(editor.deleteElement(args.value(QStringLiteral("id")).toString()));
    });

    server.registerTool(csgDescriptor(QStringLiteral("csg_union"), QStringLiteral("union")),
                        [&editor](const QJsonObject& args) {
                          return resultToJson(editor.csgUnion(
                            args.value(QStringLiteral("a")).toString(),
                            args.value(QStringLiteral("b")).toString()));
                        });

    server.registerTool(
      csgDescriptor(QStringLiteral("csg_intersect"), QStringLiteral("intersection")),
      [&editor](const QJsonObject& args) {
        return resultToJson(editor.csgIntersect(args.value(QStringLiteral("a")).toString(),
                                                args.value(QStringLiteral("b")).toString()));
      });

    server.registerTool(
      csgDescriptor(QStringLiteral("csg_difference"), QStringLiteral("difference")),
      [&editor](const QJsonObject& args) {
        return resultToJson(editor.csgDifference(args.value(QStringLiteral("a")).toString(),
                                                 args.value(QStringLiteral("b")).toString()));
      });

    server.registerTool(setCameraDescriptor(), [&editor](const QJsonObject& args) {
      return resultToJson(editor.setCamera(args.value(QStringLiteral("position")),
                                           args.value(QStringLiteral("target")),
                                           args.value(QStringLiteral("fov"))));
    });
  }

}
