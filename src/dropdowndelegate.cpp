#include "dropdowndelegate.h"
#include <QComboBox>

DropdownDelegate::DropdownDelegate(const QStringList& options, QObject* parent)
: QStyledItemDelegate(parent), options(options)
{
  // initializers only
}

DropdownDelegate::DropdownDelegate(QObject* parent)
: DropdownDelegate({}, parent)
{
  // forwarded constructor only
}

QWidget* DropdownDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const
{
  // QComboBox's USER property is not well-documented, but it's `currentText`.
  // QStyledItemDelegate knows how to connect to it to automate everything.
  QComboBox* editor = new QComboBox(parent);
  for (const QString& option : options) {
    editor->addItem(option);
  }
  editor->showPopup();
  return editor;
}
