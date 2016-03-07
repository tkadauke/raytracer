#include "widgets/world/SceneModel.h"
#include "world/objects/Element.h"

#include <QMimeData>
#include <QIODevice>
#include <QDataStream>

SceneModel::SceneModel(Element* root, QObject* parent)
  : QAbstractItemModel(parent)
{
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

  auto childItem = parentItem->childElements()[row];
  if (childItem)
    return createIndex(row, column, childItem);
  else
    return QModelIndex();
}

QModelIndex SceneModel::parent(const QModelIndex& index) const {
  if (!index.isValid())
    return QModelIndex();

  auto childItem = static_cast<Element*>(index.internalPointer());
  auto parentItem = childItem->parent();

  if (parentItem == m_rootItem)
    return QModelIndex();

  return createIndex(parentItem->row(), 0, parentItem);
}

int SceneModel::rowCount(const QModelIndex& parent) const {
  Element* parentItem;
  if (parent.column() > 0)
    return 0;

  if (!parent.isValid())
    parentItem = m_rootItem;
  else
    parentItem = static_cast<Element*>(parent.internalPointer());

  return parentItem->childElements().size();
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
    return item->name();
  } else {
    return QVariant();
  }
}

Qt::ItemFlags SceneModel::flags(const QModelIndex& index) const {
  Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);

  if (!index.isValid())
    return 0;

  return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | defaultFlags;
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

bool SceneModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) {
  Q_UNUSED(column);
  
  if (!data || action == Qt::IgnoreAction)
    return false;

  QByteArray encodedData = data->data("application/modeler.element.list");
  Element* element = (Element*)encodedData.toULongLong();
  if(!element) return false;

  QModelIndex destinationParentIndex;
  Element* parentElement = static_cast<Element*>(parent.internalPointer());
  if(parentElement) {
    destinationParentIndex = parent;
  } else {
    parentElement = m_scene;
    destinationParentIndex = createIndex(0, 0, m_scene);
  }

  if(row == -1)
    row = parentElement->childElements().size();

  Element* sourceParentElement = element->parent();
  QModelIndex sourceParent = createIndex(sourceParentElement->row(), 0, sourceParentElement);
  moveRow(sourceParent, element->row(), destinationParentIndex, row);
  return true;
}

bool SceneModel::moveRows(const QModelIndex& sourceParent, int sourceRow, int count, const QModelIndex& destinationParent, int destinationChild) {
  Element* sourceParentElement = static_cast<Element*>(sourceParent.internalPointer());
  Element* destinationParentElement = static_cast<Element*>(destinationParent.internalPointer());
  Element* childElement = sourceParentElement->childElements()[sourceRow];

  if (childElement == sourceParentElement || childElement == destinationParentElement)
    return false;

  if(sourceParentElement == destinationParentElement) {
    if (destinationChild == sourceRow && (destinationChild - 1) == sourceRow) {
      return false;
    } else {
      beginMoveRows(sourceParent, sourceRow, sourceRow + count-1, destinationParent, destinationChild);
      sourceParentElement->moveChild(sourceRow, destinationChild);
      endMoveRows();
      return true;
    }
  } else {
    if (destinationParentElement->canHaveChild(childElement)) {
      beginMoveRows(sourceParent, sourceRow, sourceRow + count-1, destinationParent, destinationChild);
      destinationParentElement->insertChild(destinationChild, childElement);
      endMoveRows();
      return true;
    } else {
      return false;
    }
  }
  return false;
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

  int row = parent->childElements().size();
  beginInsertRows(dest, row, row);
  parent->addChild(element);
  endInsertRows();
}

#include "SceneModel.moc"
