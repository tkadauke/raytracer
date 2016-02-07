#ifndef SCENE_WIDGET_H
#define SCENE_WIDGET_H

#include <QWidget>

#include <string>

class SceneWidget : public QWidget {
  Q_OBJECT
  
public:
  SceneWidget(QWidget* parent = nullptr);
  ~SceneWidget();
  
  std::string sceneName() const;

signals:
  void changed();

private slots:
  void sceneChanged();

private:
  struct Private;
  Private* p;
};

#endif
