#pragma once

#include <QVariant>
#include <QWidget>

#include <memory>

class Element;
class QDoubleSpinBox;
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

private:
  struct Private;
  std::unique_ptr<Private> p;
};
