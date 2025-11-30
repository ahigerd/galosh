#ifndef GALOSH_DROPDOWNDELEGATE_H
#define GALOSH_DROPDOWNDELEGATE_H

#include <QStyledItemDelegate>
#include <QStringList>
class MapManager;

class DropdownDelegate : public QStyledItemDelegate
{
Q_OBJECT
public:
  DropdownDelegate(QObject* parent = nullptr);
  DropdownDelegate(const QStringList& options, QObject* parent = nullptr);

  QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
  QStringList options;
};

#endif
