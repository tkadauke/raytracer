#pragma once

#include <QSizePolicy>
#include <QVariant>
#include <QWidget>

#include <memory>

class Element;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QVBoxLayout;

class AbstractParameterWidget : public QWidget {
  Q_OBJECT

public:
  explicit AbstractParameterWidget(QWidget* parent = nullptr);
  ~AbstractParameterWidget();

  void setElement(Element* element);
  const QString& parameterName() const;
  virtual void setParameterName(const QString& name);

  virtual const QVariant value() const = 0;
  virtual void setValue(const QVariant& value) = 0;

signals:
  void changed(const QString&, const QVariant&);

protected slots:
  void parameterChanged();
  QVariant lastValue() const;

protected:
  Element* element() const;
  QString displayNameForParameter(const QString& name) const;
  QString displayNameForChoice(const QString& choice) const;
  virtual void updatePropertyConfiguration();
  virtual void setLabelText(const QString& text);

  /// Builds a `QDoubleSpinBox` with the layout properties shared by the
  /// per-component edits (color channels, vector coordinates, ...): zero
  /// minimum width, an ignored/fixed size policy, and the given range/step.
  static QDoubleSpinBox* makeSpinBoxEdit(QWidget* parent, double minimum, double maximum,
                                         int decimals, double singleStep);

  /// Lays out a short label plus @p edit in a single row appended to @p layout.
  static void addLabeledSpinBoxRow(QVBoxLayout* layout, const QString& name,
                                   QDoubleSpinBox* edit, QWidget* parent);

  /// Configures the parameter-name caption label shared by the parameter
  /// widgets: no minimum width, word wrap enabled, and the given horizontal
  /// size policy (vertical stays `Preferred`).
  static void configureLabel(QLabel* label,
                             QSizePolicy::Policy horizontalPolicy = QSizePolicy::Ignored);

  /// Configures the size-adjust/minimum-content layout properties shared by
  /// the combo-box-backed parameter widgets (angle unit, choice, reference):
  /// no minimum width, a 6-character minimum content length that drives
  /// size-adjustment, and the given horizontal size policy (vertical stays
  /// `Fixed`).
  static void configureComboBox(QComboBox* comboBox,
                                QSizePolicy::Policy horizontalPolicy = QSizePolicy::Ignored);

  /// @returns true if any of @p widgets currently has keyboard focus. Used
  /// by `setValue`/`setColor`/`setVector` overrides to avoid clobbering a
  /// field the user is actively typing in — single-widget parameter
  /// widgets pass one edit, multi-component ones (color, vector) pass all
  /// of their component edits.
  template <typename... Widgets>
  static bool anyHasFocus(Widgets*... widgets) {
    return (... || widgets->hasFocus());
  }

  /// Wires each of @p edits' `valueChanged(double)` signal to
  /// `parameterChanged()`. Shared by the multi-component parameter widgets
  /// (color, vector) whose UI is a flat list of per-component spin boxes.
  template <typename... Edits>
  void connectValueChangedInputs(Edits*... edits) {
    (connect(edits, SIGNAL(valueChanged(double)), this, SLOT(parameterChanged())), ...);
  }

private:
  struct Private;
  std::unique_ptr<Private> p;
};
