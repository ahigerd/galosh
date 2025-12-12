#ifndef GALOSH_EQUIPMENTVIEW_H
#define GALOSH_EQUIPMENTVIEW_H

#include <QWidget>
#include <QMap>
#include "itemdatabase.h"
class QComboBox;

class EquipmentView : public QWidget
{
public:
  EquipmentView(ItemDatabase* db, QWidget* parent = nullptr);

  void setItems(const QList<ItemDatabase::EquipSlot>& equipment);

private:
  void showStats(QString name);

  ItemDatabase* db;
  QMap<QString, QComboBox*> slotItems;
};

#endif
