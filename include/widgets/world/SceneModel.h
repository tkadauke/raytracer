#pragma once

#include <QAbstractItemModel>

class Element;

class SceneModel : public QAbstractItemModel {
  Q_OBJECT;

public:
  SceneModel(Element* root, QObject* parent = nullptr);
  ~SceneModel();
  
  void setElement(Element* root);

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
