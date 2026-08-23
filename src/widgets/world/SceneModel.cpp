#include "widgets/world/SceneModel.h"
#include "world/objects/Element.h"

#include <QMimeData>
#include <QIODevice>
#include <QDataStream>

namespace {

bool isDescendantOf(Element* candidate, Element* ancestor) {
  for (auto* current = candidate; current; current = current->parent()) {
    if (current == ancestor)
      return true;
  }
  return false;
}

} // namespace

SceneModel::SceneModel(Element* root, QObject* parent)
    : QAbstractItemModel(parent) {
  setElement(root);
}

SceneModel::~SceneModel() {
  delete m_rootItem;
}

void SceneModel::setElement(Element* root) {
  beginResetModel();
  m_scene = root;
  m_rootItem = new Element(nullptr);
  m_rootItem->addChild(m_scene);
  endResetModel();
}

QModelIndex SceneModel::index(int row, int column, const QModelIndex& parent) const {
  if (!hasIndex(row, column, parent))
    return QModelIndex();

  Element* parentItem;

  if (!parent.isValid())
    parentItem = m_rootItem;
  else
    parentItem = static_cast<Element*>(parent.internalPointer());

  auto childItem = visibleChildAt(parentItem, row);
  if (childItem)
    return createIndex(row, column, childItem);

  return QModelIndex();
}

QModelIndex SceneModel::parent(const QModelIndex& index) const {
  if (!index.isValid())
    return QModelIndex();

  auto childItem = static_cast<Element*>(index.internalPointer());
  auto parentItem = childItem->parent();

  if (parentItem == m_rootItem)
    return QModelIndex();

  Element* grandParent = parentItem->parent();
  if (!grandParent)
    return QModelIndex();

  return createIndex(visibleRowOf(grandParent, parentItem), 0, parentItem);
}

int SceneModel::rowCount(const QModelIndex& parent) const {
  Element* parentItem;
  if (parent.column() > 0)
    return 0;

  if (!parent.isValid())
    parentItem = m_rootItem;
  else
    parentItem = static_cast<Element*>(parent.internalPointer());

  return visibleChildCount(parentItem);
}

int SceneModel::columnCount(const QModelIndex&) const {
  return 1;
}

QVariant SceneModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid())
    return QVariant();

  if (role != Qt::DisplayRole)
    return QVariant();

  auto item = static_cast<Element*>(index.internalPointer());

  if (index.column() == 0) {
    return item->displayName();
  } else {
    return QVariant();
  }
}

Qt::ItemFlags SceneModel::flags(const QModelIndex& index) const {
  Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);

  if (!index.isValid())
    return Qt::ItemFlags();

  auto item = static_cast<Element*>(index.internalPointer());
  if (item && item->isGenerated())
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | defaultFlags;

  return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled |
         defaultFlags;
}

QVariant SceneModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    QList<QVariant> rootData;
    rootData << "Name";

    return rootData[section];
  }

  return QVariant();
}

Qt::DropActions SceneModel::supportedDropActions() const {
  return Qt::MoveAction;
}

QStringList SceneModel::mimeTypes() const {
  QStringList types;
  types << "application/modeler.element.list";
  return types;
}

QMimeData* SceneModel::mimeData(const QModelIndexList& indexes) const {
  if (indexes.size() != 1 || !indexes[0].isValid())
    return nullptr;

  quintptr elementAddress = (quintptr)indexes[0].internalPointer();
  QByteArray encodedData(QString::number(elementAddress).toUtf8());

  QMimeData* mimeData = new QMimeData();
  mimeData->setData("application/modeler.element.list", encodedData);
  return mimeData;
}

bool SceneModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                              const QModelIndex& parent) {
  Q_UNUSED(column);

  if (!data || action == Qt::IgnoreAction)
    return false;

  if (action != Qt::MoveAction)
    return false;

  QByteArray encodedData = data->data("application/modeler.element.list");
  Element* element = reinterpret_cast<Element*>(encodedData.toULongLong());
  if (!element)
    return false;

  QModelIndex destinationParentIndex;
  Element* parentElement = static_cast<Element*>(parent.internalPointer());
  if (parentElement) {
    destinationParentIndex = parent;
  } else {
    parentElement = m_scene;
    destinationParentIndex = createIndex(0, 0, m_scene);
  }

  if (row == -1)
    row = visibleChildCount(parentElement);

  Element* sourceParentElement = element->parent();
  if (!sourceParentElement || !parentElement->canHaveChild(element))
    return false;

  QModelIndex sourceParent = createIndex(
    visibleRowOf(sourceParentElement->parent(), sourceParentElement), 0, sourceParentElement);
  return moveRow(sourceParent, visibleRowOf(sourceParentElement, element), destinationParentIndex,
                 row);
}

bool SceneModel::moveRows(const QModelIndex& sourceParent, int sourceRow, int count,
                          const QModelIndex& destinationParent, int destinationChild) {
  if (count != 1)
    return false;

  Element* sourceParentElement = static_cast<Element*>(sourceParent.internalPointer());
  Element* destinationParentElement = static_cast<Element*>(destinationParent.internalPointer());
  if (!sourceParentElement || !destinationParentElement)
    return false;

  if (sourceRow < 0 || sourceRow >= visibleChildCount(sourceParentElement))
    return false;

  if (destinationChild < 0 || destinationChild > visibleChildCount(destinationParentElement))
    return false;

  Element* childElement = visibleChildAt(sourceParentElement, sourceRow);
  if (!childElement)
    return false;
  const int sourceChild = sourceParentElement->childElements().indexOf(childElement);
  const int destinationPhysicalChild =
    insertionRowForVisibleRow(destinationParentElement, destinationChild);

  if (childElement == sourceParentElement || childElement == destinationParentElement)
    return false;

  if (isDescendantOf(destinationParentElement, childElement))
    return false;

  if (!destinationParentElement->canHaveChild(childElement))
    return false;

  if (sourceParentElement == destinationParentElement) {
    if (destinationChild == sourceRow || destinationChild == sourceRow + 1) {
      return false;
    } else {
      beginMoveRows(sourceParent, sourceRow, sourceRow + count - 1, destinationParent,
                    destinationChild);
      sourceParentElement->moveChild(sourceChild, destinationPhysicalChild);
      endMoveRows();
      return true;
    }
  } else {
    beginMoveRows(sourceParent, sourceRow, sourceRow + count - 1, destinationParent,
                  destinationChild);
    destinationParentElement->insertChild(destinationPhysicalChild, childElement);
    endMoveRows();
    return true;
  }
}

void SceneModel::deleteElement(const QModelIndex& index) {
  Element* element = static_cast<Element*>(index.internalPointer());

  beginRemoveRows(index.parent(), index.row(), index.row());
  if (element->parent()) {
    element->parent()->removeChild(element, false);
  }
  element->unlink(m_scene);
  delete element;
  endRemoveRows();
}

void SceneModel::addElement(const QModelIndex& index, Element* element) {
  Element* parent = m_scene;
  QModelIndex dest = index;
  if (index.isValid()) {
    parent = static_cast<Element*>(index.internalPointer());
    while (!parent->canHaveChild(element)) {
      parent = parent->parent();
      dest = dest.parent();
    }
  }

  int row = visibleChildCount(parent);
  beginInsertRows(dest, row, row);
  parent->addChild(element);
  endInsertRows();
}

QModelIndex SceneModel::indexForElement(Element* element) const {
  if (!element || element == m_rootItem)
    return QModelIndex();

  QList<Element*> chain;
  for (Element* current = element; current && current != m_rootItem; current = current->parent())
    chain.prepend(current);

  if (chain.isEmpty() || chain.first()->parent() != m_rootItem)
    return QModelIndex();

  QModelIndex result;
  for (Element* node : chain) {
    Element* parentItem = node->parent();
    if (!parentItem->displayInSceneModel() && parentItem != m_rootItem)
      return QModelIndex();
    if (!node->displayInSceneModel())
      return QModelIndex();

    const int row = visibleRowOf(parentItem, node);
    result = index(row, 0, result);
    if (!result.isValid())
      return QModelIndex();
  }
  return result;
}

int SceneModel::visibleChildCount(Element* parent) const {
  int result = 0;
  for (Element* child : parent->childElements()) {
    if (child->displayInSceneModel())
      ++result;
  }
  return result;
}

Element* SceneModel::visibleChildAt(Element* parent, int row) const {
  int visibleRow = 0;
  for (Element* child : parent->childElements()) {
    if (!child->displayInSceneModel())
      continue;
    if (visibleRow == row)
      return child;
    ++visibleRow;
  }
  return nullptr;
}

int SceneModel::visibleRowOf(Element* parent, Element* child) const {
  if (!parent)
    return 0;

  int visibleRow = 0;
  for (Element* candidate : parent->childElements()) {
    if (!candidate->displayInSceneModel())
      continue;
    if (candidate == child)
      return visibleRow;
    ++visibleRow;
  }
  return 0;
}

int SceneModel::insertionRowForVisibleRow(Element* parent, int row) const {
  if (row < 0)
    return parent->childElements().size();

  int visibleRow = 0;
  const auto children = parent->childElements();
  for (int i = 0; i != children.size(); ++i) {
    if (!children[i]->displayInSceneModel())
      continue;
    if (visibleRow == row)
      return i;
    ++visibleRow;
  }
  return children.size();
}
