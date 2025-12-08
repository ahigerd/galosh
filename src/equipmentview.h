#ifndef GALOSH_EQUIPMENTVIEW_H
#define GALOSH_EQUIPMENTVIEW_H

#include <QWidget>
#include "itemdatabase.h"

class EquipmentView : public QWidget
{
public:
  EquipmentView(const QList<ItemDatabase::EquipSlot>& equipment, QWidget* parent = nullptr);
};

#endif
