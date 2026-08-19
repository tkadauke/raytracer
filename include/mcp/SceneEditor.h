#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>
#include <functional>
#include <memory>

class Element;
class Scene;
class SceneModel;
class QItemSelectionModel;

namespace mcp {

  /**
    * Result of a single mcp::SceneEditor mutation.
    */
  struct EditResult {
    bool ok = false;
    /// The id of the element the call created or otherwise names, when
    /// applicable (e.g. the new primitive's id for addPrimitive(), the new
    /// CSG node's id for csgUnion()/csgIntersect()/csgDifference()).
    QString id;
    /// Populated with the failure reason when `ok` is false, or an optional
    /// informational note (e.g. "fov not supported by this camera type")
    /// when `ok` is true.
    QString message;
  };

  /**
    * Applies mutating scene edits (roadmap §4.6.i, "v1 tool surface") through
    * the exact same `SceneModel` / `QItemSelectionModel` machinery the
    * Modeler's UI uses, so an MCP-driven edit fires the identical structural
    * (`rowsInserted`/`rowsRemoved`/`moveRows`) and selection
    * (`QItemSelectionModel::currentChanged`) signals a menu action would —
    * see `MainWindow::add<T>()`, `MainWindow::deleteElement()`, and the
    * drag-and-drop reparenting path in `SceneModel::dropMimeData()` for the
    * UI-side counterparts each method here mirrors.
    *
    * Property edits (transform, apply_material, set_camera) reuse
    * `Element::read()` — the same generic JSON-to-Q_PROPERTY parser scene
    * loading uses — instead of hand-rolling per-property JSON conversion.
    *
    * Owns none of `sceneModel`/`selectionModel`; the constructor's caller
    * (MainWindow, or a test fixture) keeps them alive for the SceneEditor's
    * lifetime and wires up its own redraw/property-sync reaction to
    * elementChanged().
    */
  class SceneEditor : public QObject {
    Q_OBJECT

  public:
    using SceneProvider = std::function<Scene*()>;

    /**
      * @p sceneProvider is invoked fresh for every mutation, so it should
      * return whatever scene is currently live — MainWindow's `p->scene`
      * pointer is replaced (the old Scene deleted) across File > New /
      * Open, so capturing a raw `Scene*` once at construction would leave
      * SceneEditor mutating a dangling scene after the first File > New.
      */
    SceneEditor(SceneProvider sceneProvider, SceneModel* sceneModel,
               QItemSelectionModel* selectionModel, QObject* parent = nullptr);

    /**
      * Creates a Box/Sphere/Cylinder/Ring/Torus primitive and adds it to the
      * scene root via `SceneModel::addElement()`, then applies @p position
      * (if the created type is Transformable) and the remaining @p params
      * (arbitrary Q_PROPERTY overrides, in the same shape as native scene
      * JSON — e.g. `{"size": [1,1,1], "material": "<id>"}`).
      */
    EditResult addPrimitive(const QString& type, const QJsonValue& position,
                            const QJsonObject& params);

    /**
      * Sets any subset of @p translate / @p rotate / @p scale (each a
      * 3-element JSON array, in the same units as native scene JSON —
      * rotation is radians) on the Transformable element named by @p id.
      */
    EditResult transform(const QString& id, const QJsonValue& translate, const QJsonValue& rotate,
                         const QJsonValue& scale);

    /**
      * Attaches a material to the Surface named by @p id. @p materialRefOrInline
      * is either a JSON string naming an existing material element's id, or
      * an inline `{"type": "MatteMaterial"|"PhongMaterial"|"TransparentMaterial",
      * "params": {...}}` object, in which case a new material element is
      * created and added to the scene root first.
      */
    EditResult applyMaterial(const QString& id, const QJsonValue& materialRefOrInline);

    /**
      * Drives the same QItemSelectionModel the Elements dock's QTreeView
      * uses, so selecting by id fires the identical
      * `currentChanged()`/`MainWindow::elementSelected()` path a click
      * would. An empty @p id clears the selection.
      */
    EditResult select(const QString& id);

    /// Deletes the element named by @p id via `SceneModel::deleteElement()`.
    EditResult deleteElement(const QString& id);

    /// Creates a Union node and reparents @p a and @p b under it.
    EditResult csgUnion(const QString& a, const QString& b);
    /// Creates an Intersection node and reparents @p a and @p b under it.
    EditResult csgIntersect(const QString& a, const QString& b);
    /// Creates a Difference node and reparents @p a and @p b under it.
    EditResult csgDifference(const QString& a, const QString& b);

    /**
      * Sets any subset of @p position / @p target (3-element JSON arrays) on
      * the scene's active camera. @p fov (degrees) is only applied when the
      * active camera type exposes a `fieldOfView` property (e.g.
      * FishEyeCamera); other camera types have no single equivalent
      * property, so `fov` is silently skipped for them (noted in the
      * returned EditResult::message).
      */
    EditResult setCamera(const QJsonValue& position, const QJsonValue& target,
                         const QJsonValue& fov);

  signals:
    /**
      * Mirrors `PropertyEditorWidget::changed(Element*)` — the signal that
      * drives `MainWindow`'s private `elementChanged()` slot (scene
      * "changed" flag, viewport redraw, `currentElementChanged()`). Fired
      * after any mutation that leaves an element's properties or the tree
      * structure different. @p element is null for a delete.
      */
    void elementChanged(Element* element);

  private:
    EditResult csgCombine(const QString& typeName, const QString& a, const QString& b);
    Element* insertElement(std::unique_ptr<Element> element);
    bool reparent(Element* element, Element* newParent);
    Scene* scene() const;

    SceneProvider m_sceneProvider;
    SceneModel* m_sceneModel;
    QItemSelectionModel* m_selectionModel;
  };

}
