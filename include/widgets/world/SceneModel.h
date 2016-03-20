#pragma once

#include <QAbstractItemModel>

class Element;

class SceneModel : public QAbstractItemModel {
  Q_OBJECT;

public:
  explicit SceneModel(Element* root, QObject* parent = nullptr);
  ~SceneModel();
  
  void setElement(Element* root);

  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
  Qt::ItemFlags flags(const QModelIndex& index) const;
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
  QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const;
  QModelIndex parent(const QModelIndex& index) const;
  int rowCount(const QModelIndex& parent = QModelIndex()) const;
  int columnCount(const QModelIndex& parent = QModelIndex()) const;

  Qt::DropActions supportedDropActions() const;
  QStringList mimeTypes() const;
  QMimeData* mimeData(const QModelIndexList& indexes) const;
  bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent);

  virtual bool moveRows(const QModelIndex& sourceParent, int sourceRow, int count, const QModelIndex& destinationParent, int destinationChild);
  
  void deleteElement(const QModelIndex& index);
  void addElement(const QModelIndex& index, Element* element);

private:
  Element* m_rootItem;
  Element* m_scene;
};
