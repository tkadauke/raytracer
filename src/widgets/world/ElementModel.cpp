#include "widgets/world/ElementModel.h"
#include "world/objects/Element.h"

ElementModel::ElementModel(Element* root, QObject* parent)
  : QAbstractItemModel(parent)
{
  m_rootItem = root;
}

ElementModel::~ElementModel() {
  delete m_rootItem;
}

void ElementModel::setElement(Element* root) {
  beginResetModel();
  m_rootItem = root;
  endResetModel();
}

QModelIndex ElementModel::index(int row, int column, const QModelIndex& parent) const {
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

QModelIndex ElementModel::parent(const QModelIndex& index) const {
  if (!index.isValid())
    return QModelIndex();

  auto childItem = static_cast<Element*>(index.internalPointer());
  auto parentItem = static_cast<Element*>(childItem->parent());

  if (parentItem == m_rootItem)
    return QModelIndex();

  return createIndex(parentItem->row(), 0, parentItem);
}

int ElementModel::rowCount(const QModelIndex& parent) const {
  Element* parentItem;
  if (parent.column() > 0)
    return 0;

  if (!parent.isValid())
    parentItem = m_rootItem;
  else
    parentItem = static_cast<Element*>(parent.internalPointer());

  return parentItem->children().size();
}

int ElementModel::columnCount(const QModelIndex&) const {
  return 2;
}

QVariant ElementModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid())
    return QVariant();

  if (role != Qt::DisplayRole)
    return QVariant();

  auto item = static_cast<Element*>(index.internalPointer());

  if (index.column() == 0) {
    return item->metaObject()->className();
  } else if (index.column() == 1) {
    return item->name();
  } else {
    return QVariant();
  }
}

Qt::ItemFlags ElementModel::flags(const QModelIndex& index) const {
  if (!index.isValid())
    return 0;

  return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant ElementModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    QList<QVariant> rootData;
    rootData << "Type" << "Name";
    
    return rootData[section];
  }

  return QVariant();
}

#include "ElementModel.moc"
