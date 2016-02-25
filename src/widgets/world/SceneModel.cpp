#include "widgets/world/SceneModel.h"
#include "world/objects/Element.h"

SceneModel::SceneModel(Element* root, QObject* parent)
  : QAbstractItemModel(parent)
{
  m_rootItem = root;
}

SceneModel::~SceneModel() {
  delete m_rootItem;
}

void SceneModel::setElement(Element* root) {
  beginResetModel();
  m_rootItem = root;
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

  auto childItem = static_cast<Element*>(parentItem->children()[row]);
  if (childItem)
    return createIndex(row, column, childItem);
  else
    return QModelIndex();
}

QModelIndex SceneModel::parent(const QModelIndex& index) const {
  if (!index.isValid())
    return QModelIndex();

  auto childItem = static_cast<Element*>(index.internalPointer());
  auto parentItem = static_cast<Element*>(childItem->parent());

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

  return parentItem->children().size();
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
  if (!index.isValid())
    return 0;

  return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant SceneModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    QList<QVariant> rootData;
    rootData << "Name";
    
    return rootData[section];
  }

  return QVariant();
}

#include "SceneModel.moc"
