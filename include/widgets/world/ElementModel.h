#ifndef ELEMENT_MODEL_H
#define ELEMENT_MODEL_H

#include <QAbstractItemModel>

class Element;

class ElementModel : public QAbstractItemModel {
  Q_OBJECT

public:
  ElementModel(Element* root, QObject* parent = 0);
  ~ElementModel();

  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
  Qt::ItemFlags flags(const QModelIndex& index) const;
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
  QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const;
  QModelIndex parent(const QModelIndex& index) const;
  int rowCount(const QModelIndex& parent = QModelIndex()) const;
  int columnCount(const QModelIndex& parent = QModelIndex()) const;

private:
  Element* m_rootItem;
};

#endif
