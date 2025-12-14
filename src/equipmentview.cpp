#include "equipmentview.h"
#include "itemsearchdialog.h"
#include <QGridLayout>
#include <QComboBox>
#include <QLabel>
#include <QToolButton>
#include <QMessageBox>
#include <QInputDialog>
#include <QMenu>
#include <QCursor>
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
          dropdown->addItem(stats.uniqueName);
        }
      }
      QToolButton* statButton = new QToolButton(this);
      statButton->setText("...");
      layout->addWidget(new QLabel(location, this), row, 0);
      layout->addWidget(dropdown, row, 1);
      layout->addWidget(statButton, row, 2);
      if (i > 0) {
        suffix = "." + QString::number(i + 1);
      }
      QString slotName = location + suffix;
      dropdown->setObjectName(slotName);
      slotItems[slotName] = dropdown;
      ++row;
      QObject::connect(statButton, &QToolButton::clicked, [this, slotName]{ showMenu(slotName); });
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

void EquipmentView::showMenu(const QString& slotName)
{
  QString slotKeyword = db->equipmentSlotType(slotName.section('.', 0, 0)).keyword;
  QString currentItem = slotItems[slotName]->currentText();
  QMenu menu;
  menu.addSection(currentItem);

  QAction* detailsAction = menu.addAction("Sh&ow Details...");
  if (currentItem.isEmpty()) {
    detailsAction->setEnabled(false);
  }

  QAction* keywordAction = menu.addAction("Set &Keyword...");
  QString itemKeyword = db->itemKeyword(currentItem);
  if (!itemKeyword.isEmpty()) {
    keywordAction->setText(QStringLiteral("&Keyword: %1").arg(itemKeyword.replace("&", "&&")));
  }

  QAction* searchAction = menu.addAction("&Search for Items...");

  QAction* action = menu.exec(QCursor::pos());
  if (action == detailsAction) {
    showStats(currentItem);
  } else if (action == keywordAction) {
    bool ok = false;
    QString newKeyword = QInputDialog::getText(
      this,
      "Set Keyword",
      QStringLiteral("Keyword for %1:").arg(currentItem),
      QLineEdit::Normal,
      itemKeyword,
      &ok
    );
    if (ok) {
      db->setItemKeyword(currentItem, newKeyword);
    }
  } else if (action == searchAction) {
    ItemSearchDialog dlg(db, true, this);
    dlg.setRequiredSlot(slotKeyword);
    if (dlg.exec() != QDialog::Accepted) {
      return;
    }
    QString item = dlg.selectedItemName();
    if (item.isEmpty()) {
      return;
    }
    QComboBox* dropdown = slotItems.value(slotName);
    int index = dropdown->findText(item);
    if (index >= 0) {
      dropdown->setCurrentIndex(index);
    }
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
  for (int id : db->searchForName({ QRegularExpression::escape(name) })) {
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
    QMessageBox mb(QMessageBox::NoIcon, "Item Details", stats, QMessageBox::Ok, this);
    mb.exec();
  }
}
