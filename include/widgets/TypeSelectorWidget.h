#pragma once

#include <QWidget>

#include <list>
#include <string>

class QComboBox;

/**
  * @brief Shared plumbing for the factory-backed "pick a type" combo-box
  *        widgets (`CameraTypeWidget`, `ViewPlaneTypeWidget`).
  *
  * Subclasses call `populateComboBox()` once their `Ui::` combo box exists,
  * passing the identifiers from their respective factory. The combo box's
  * `activated(int)` signal is wired to the inherited `typeChanged()` slot,
  * which re-emits `changed()`.
  */
class TypeSelectorWidget : public QWidget {
  Q_OBJECT

public:
  explicit TypeSelectorWidget(QWidget* parent = nullptr);

signals:
  void changed();

protected slots:
  void typeChanged();

protected:
  void populateComboBox(QComboBox* comboBox, const std::list<std::string>& identifiers);
};
