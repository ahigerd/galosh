#include "equipmentview.h"
#include <QGridLayout>
#include <QComboBox>
#include <QLabel>
#include <QToolButton>
#include <QMessageBox>
#include <QtDebug>

EquipmentView::EquipmentView(ItemDatabase* db, QWidget* parent)
: QWidget(parent), db(db)
{
  QGridLayout* layout = new QGridLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setVerticalSpacing(0);

  int row = 0;
  for (const QString& location : db->equipmentSlotOrder()) {
    auto slot = db->equipmentSlotType(location);
    QString suffix;
    QString keyword = db->equipmentSlotType(location).keyword;
    for (int i = 0; i < slot.count; i++) {
      QComboBox* dropdown = new QComboBox(this);
      dropdown->addItem("");
      if (!keyword.isEmpty()) {
        for (const auto& stats : db->searchForItem({{ "worn", keyword }})) {
          dropdown->addItem(stats.name);
        }
      }
      QToolButton* statButton = new QToolButton(this);
      statButton->setText("...");
      QObject::connect(statButton, &QToolButton::clicked, [this, dropdown]{ showStats(dropdown->currentText()); });
      layout->addWidget(new QLabel(location, this), row, 0);
      layout->addWidget(dropdown, row, 1);
      layout->addWidget(statButton, row, 2);
      if (i > 0) {
        suffix = "." + QString::number(i + 1);
      }
      dropdown->setObjectName(location + suffix);
      slotItems[location + suffix] = dropdown;
      ++row;
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
  QFont bold = font();
  bold.setBold(true);
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
      dropdown->addItem(item.displayName + " *");
      dropdown->setCurrentText(item.displayName + " *");
    }
    dropdown->setItemData(dropdown->currentIndex(), QVariant::fromValue(bold), Qt::FontRole);
  }
}

void EquipmentView::showStats(QString name)
{
  if (name.isEmpty()) {
    return;
  }
  if (name.endsWith(" *")) {
    name = name.mid(0, name.length() - 2);
  }

  QString stats;
  int count = 0;
  for (int id : db->searchForName({ name })) {
    ++count;
    if (!stats.isEmpty()) {
      stats += "<br/><br/>";
    }
    QString itemName = db->itemName(id);
    stats += QStringLiteral("<b>Object name: %1</b><br/>").arg(itemName);
    QString keyword = db->itemKeyword(itemName);
    if (!keyword.isEmpty()) {
      stats += QStringLiteral("Assigned keyword: %1<br/>").arg(keyword);
    }
    stats += db->itemStats(itemName).trimmed().replace("\n", "<br/>");
  }
  if (stats.isEmpty()) {
    QMessageBox::warning(this, "Galosh", QStringLiteral("No item data found for \"%1\".").arg(name));
  } else {
    if (count > 1) {
      stats = QStringLiteral("%1 matching items found:<br/><br/>%2").arg(name).arg(stats);
    }
    QMessageBox::information(this, "Galosh", stats);
  }
}
