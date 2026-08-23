#include "mcp/SceneEditor.h"

#include "widgets/world/SceneModel.h"

#include "world/objects/Camera.h"
#include "world/objects/Difference.h"
#include "world/objects/Element.h"
#include "world/objects/ElementFactory.h"
#include "world/objects/Intersection.h"
#include "world/objects/Scene.h"
#include "world/objects/Surface.h"
#include "world/objects/Transformable.h"
#include "world/objects/Union.h"
#include "core/math/Angle.h"

#include <QItemSelectionModel>
#include <QSet>
#include <utility>

namespace mcp {

  namespace {
    const QSet<QString>& allowedPrimitiveTypes() {
      static const QSet<QString> types = {QStringLiteral("Box"), QStringLiteral("Sphere"),
                                          QStringLiteral("Cylinder"), QStringLiteral("Ring"),
                                          QStringLiteral("Torus")};
      return types;
    }

    const QSet<QString>& allowedMaterialTypes() {
      static const QSet<QString> types = {QStringLiteral("MatteMaterial"),
                                          QStringLiteral("PhongMaterial"),
                                          QStringLiteral("TransparentMaterial")};
      return types;
    }

    EditResult failure(const QString& message) {
      return EditResult{false, QString(), message};
    }

    EditResult success(const QString& id = QString(), const QString& message = QString()) {
      return EditResult{true, id, message};
    }
  }

  SceneEditor::SceneEditor(SceneProvider sceneProvider, SceneModel* sceneModel,
                           QItemSelectionModel* selectionModel, QObject* parent)
      : QObject(parent),
        m_sceneProvider(std::move(sceneProvider)),
        m_sceneModel(sceneModel),
        m_selectionModel(selectionModel) {
  }

  Scene* SceneEditor::scene() const {
    return m_sceneProvider ? m_sceneProvider() : nullptr;
  }

  Element* SceneEditor::insertElement(std::unique_ptr<Element> element) {
    Element* raw = element.release();
    m_sceneModel->addElement(QModelIndex(), raw);
    return raw;
  }

  bool SceneEditor::reparent(Element* element, Element* newParent) {
    const QModelIndex elementIndex = m_sceneModel->indexForElement(element);
    if (!elementIndex.isValid())
      return false;

    const QModelIndex destParentIndex = m_sceneModel->indexForElement(newParent);
    const int destRow = m_sceneModel->rowCount(destParentIndex);
    return m_sceneModel->moveRow(elementIndex.parent(), elementIndex.row(), destParentIndex,
                                 destRow);
  }

  EditResult SceneEditor::addPrimitive(const QString& type, const QJsonValue& position,
                                       const QJsonObject& params) {
    Scene* liveScene = scene();
    if (!liveScene)
      return failure(QStringLiteral("No scene is currently open"));

    if (!allowedPrimitiveTypes().contains(type))
      return failure(QStringLiteral("Unsupported primitive type: %1").arg(type));

    auto created = ElementFactory::self().create(type.toStdString());
    if (!created)
      return failure(QStringLiteral("Unknown element type: %1").arg(type));

    Element* element = insertElement(std::move(created));
    element->setName(QStringLiteral("%1 %2")
                       .arg(element->metaObject()->className())
                       .arg(liveScene->childElements().size()));

    QJsonObject json = params;
    if (!position.isUndefined() && !position.isNull() && dynamic_cast<Transformable*>(element))
      json[QStringLiteral("position")] = position;

    element->read(json);
    liveScene->resolveElementReferences();

    emit elementChanged(element);
    return success(element->id());
  }

  EditResult SceneEditor::transform(const QString& id, const QJsonValue& translate,
                                    const QJsonValue& rotate, const QJsonValue& scale) {
    Scene* liveScene = scene();
    Element* element = liveScene ? liveScene->findById(id) : nullptr;
    if (!element)
      return failure(QStringLiteral("No element with id %1").arg(id));

    if (!dynamic_cast<Transformable*>(element))
      return failure(QStringLiteral("Element %1 is not transformable").arg(id));

    QJsonObject json;
    if (!translate.isUndefined())
      json[QStringLiteral("position")] = translate;
    if (!rotate.isUndefined())
      json[QStringLiteral("rotation")] = rotate;
    if (!scale.isUndefined())
      json[QStringLiteral("scale")] = scale;

    if (json.isEmpty())
      return failure(QStringLiteral("transform requires at least one of translate/rotate/scale"));

    element->read(json);

    emit elementChanged(element);
    return success(id);
  }

  EditResult SceneEditor::applyMaterial(const QString& id, const QJsonValue& materialRefOrInline) {
    Scene* liveScene = scene();
    if (!liveScene)
      return failure(QStringLiteral("No scene is currently open"));

    Element* target = liveScene->findById(id);
    auto* surface = dynamic_cast<Surface*>(target);
    if (!surface)
      return failure(QStringLiteral("Element %1 is not a surface").arg(id));

    QString materialId;
    if (materialRefOrInline.isString()) {
      materialId = materialRefOrInline.toString();
      if (!liveScene->findById(materialId))
        return failure(QStringLiteral("No material with id %1").arg(materialId));
    } else if (materialRefOrInline.isObject()) {
      const QJsonObject inlineMaterial = materialRefOrInline.toObject();
      const QString materialType = inlineMaterial.value(QStringLiteral("type")).toString();
      if (!allowedMaterialTypes().contains(materialType))
        return failure(QStringLiteral("Unsupported material type: %1").arg(materialType));

      auto created = ElementFactory::self().create(materialType.toStdString());
      if (!created)
        return failure(QStringLiteral("Unknown material type: %1").arg(materialType));

      Element* materialElement = insertElement(std::move(created));
      materialElement->setName(QStringLiteral("%1 %2")
                                 .arg(materialElement->metaObject()->className())
                                 .arg(liveScene->childElements().size()));
      materialElement->read(inlineMaterial.value(QStringLiteral("params")).toObject());
      materialId = materialElement->id();
    } else {
      return failure(
        QStringLiteral("material must be an id string or an inline {type, params} object"));
    }

    surface->read(QJsonObject{{QStringLiteral("material"), materialId}});
    liveScene->resolveElementReferences();

    emit elementChanged(surface);
    return success(materialId);
  }

  EditResult SceneEditor::select(const QString& id) {
    if (id.isEmpty()) {
      m_selectionModel->clearSelection();
      m_selectionModel->setCurrentIndex(QModelIndex(), QItemSelectionModel::Clear);
      return success();
    }

    Scene* liveScene = scene();
    Element* element = liveScene ? liveScene->findById(id) : nullptr;
    if (!element)
      return failure(QStringLiteral("No element with id %1").arg(id));

    const QModelIndex index = m_sceneModel->indexForElement(element);
    if (!index.isValid())
      return failure(QStringLiteral("Element %1 is not part of the scene tree").arg(id));

    m_selectionModel->setCurrentIndex(index,
                                      QItemSelectionModel::ClearAndSelect |
                                        QItemSelectionModel::Current);
    return success(id);
  }

  EditResult SceneEditor::deleteElement(const QString& id) {
    Scene* liveScene = scene();
    Element* element = liveScene ? liveScene->findById(id) : nullptr;
    if (!element)
      return failure(QStringLiteral("No element with id %1").arg(id));

    if (element == liveScene)
      return failure(QStringLiteral("Cannot delete the scene root"));

    const QModelIndex index = m_sceneModel->indexForElement(element);
    if (!index.isValid())
      return failure(QStringLiteral("Element %1 is not part of the scene tree").arg(id));

    m_sceneModel->deleteElement(index);

    emit elementChanged(nullptr);
    return success();
  }

  EditResult SceneEditor::csgCombine(const QString& typeName, const QString& aId,
                                     const QString& bId) {
    Scene* liveScene = scene();
    if (!liveScene)
      return failure(QStringLiteral("No scene is currently open"));

    Element* a = liveScene->findById(aId);
    if (!dynamic_cast<Surface*>(a))
      return failure(QStringLiteral("No surface with id %1").arg(aId));

    Element* b = liveScene->findById(bId);
    if (!dynamic_cast<Surface*>(b))
      return failure(QStringLiteral("No surface with id %1").arg(bId));

    if (a == b)
      return failure(QStringLiteral("csg operands must be different elements"));

    auto created = ElementFactory::self().create(typeName.toStdString());
    if (!created)
      return failure(QStringLiteral("Unknown CSG type: %1").arg(typeName));

    Element* csgElement = insertElement(std::move(created));
    csgElement->setName(QStringLiteral("%1 %2")
                          .arg(csgElement->metaObject()->className())
                          .arg(liveScene->childElements().size()));

    if (!reparent(a, csgElement) || !reparent(b, csgElement)) {
      return failure(
        QStringLiteral("Could not move %1/%2 under the new %3").arg(aId, bId, typeName));
    }

    emit elementChanged(csgElement);
    return success(csgElement->id());
  }

  EditResult SceneEditor::csgUnion(const QString& a, const QString& b) {
    return csgCombine(QStringLiteral("Union"), a, b);
  }

  EditResult SceneEditor::csgIntersect(const QString& a, const QString& b) {
    return csgCombine(QStringLiteral("Intersection"), a, b);
  }

  EditResult SceneEditor::csgDifference(const QString& a, const QString& b) {
    return csgCombine(QStringLiteral("Difference"), a, b);
  }

  EditResult SceneEditor::setCamera(const QJsonValue& position, const QJsonValue& target,
                                    const QJsonValue& fov) {
    Scene* liveScene = scene();
    Camera* camera = liveScene ? liveScene->activeCamera() : nullptr;
    if (!camera)
      return failure(QStringLiteral("Scene has no camera"));

    QJsonObject json;
    if (!position.isUndefined())
      json[QStringLiteral("position")] = position;
    if (!target.isUndefined())
      json[QStringLiteral("target")] = target;

    const bool fovRequested = !fov.isUndefined();
    const bool fovSupported = fovRequested && camera->metaObject()->indexOfProperty("fieldOfView") >= 0;
    if (fovSupported)
      json[QStringLiteral("fieldOfView")] = Angled::fromDegrees(fov.toDouble()).radians();

    const QString unsupportedFovNote =
      (fovRequested && !fovSupported)
        ? QStringLiteral("%1 has no single field-of-view property; fov was not applied")
            .arg(camera->metaObject()->className())
        : QString();

    if (json.isEmpty()) {
      return failure(unsupportedFovNote.isEmpty()
                       ? QStringLiteral("set_camera requires at least one of position/target/fov")
                       : unsupportedFovNote);
    }

    camera->read(json);

    emit elementChanged(camera);
    return success(camera->id(), unsupportedFovNote);
  }

}
