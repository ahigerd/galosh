#ifndef GALOSH_EQUIPMENTVIEW_H
#define GALOSH_EQUIPMENTVIEW_H

#include <QWidget>
#include <QMap>
#include "itemdatabase.h"
class QComboBox;

class EquipmentView : public QWidget
{
Q_OBJECT
public:
  EquipmentView(ItemDatabase* db, QWidget* parent = nullptr);

  ItemDatabase::EquipmentSet items() const;
  void setItems(const ItemDatabase::EquipmentSet& equipment, bool ignoreEmpty = false);

  void showStats(const QString& name);
  void promptForKeyword(const QString& name, const QString& current);

private slots:
  void onItemChanged(QComboBox* dropdown = nullptr);

private:
  void showMenu(const QString& slotName);

  ItemDatabase* db;
  QMap<QString, QComboBox*> slotItems;
};

#endif
