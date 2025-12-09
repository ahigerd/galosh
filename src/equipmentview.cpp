#include "equipmentview.h"
#include <QFormLayout>
#include <QComboBox>
#include <QtDebug>

EquipmentView::EquipmentView(ItemDatabase* db, QWidget* parent)
: QWidget(parent), db(db)
{
  QFormLayout* layout = new QFormLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setVerticalSpacing(0);

  for (const QString& location : db->equipmentSlotOrder()) {
    auto slot = db->equipmentSlotType(location);
    QString suffix;
    for (int i = 0; i < slot.count; i++) {
      QComboBox* dropdown = new QComboBox(this);
      dropdown->addItem("");
      layout->addRow(location, dropdown);
      if (i > 0) {
        suffix = "." + QString::number(i + 1);
      }
      dropdown->setObjectName(location + suffix);
      slotItems[location + suffix] = dropdown;
    }
  }
}

void EquipmentView::setItems(const QList<ItemDatabase::EquipSlot>& equipment)
{
  int index = 1;
  QString lastSlot;
  for (QComboBox* dropdown : slotItems) {
    dropdown->setCurrentIndex(-1);
  }
  for (const ItemDatabase::EquipSlot& item : equipment) {
    QString suffix;
    if (item.location == lastSlot) {
      index++;
      suffix = "." + QString::number(index);
    } else {
      index = 1;
      lastSlot = item.location;
    }
    QComboBox* dropdown = slotItems.value(item.location + suffix);
    if (!dropdown) {
      qWarning() << "XXX: more items equipped than available slots";
      continue;
    }
    dropdown->setCurrentText(item.displayName);
    if (dropdown->currentIndex() < 0) {
      dropdown->addItem(item.displayName);
      dropdown->setCurrentText(item.displayName);
    }
  }
}
