#include "equipmentview.h"
#include "itemsearchdialog.h"
#include "algorithms.h"
#include <QGridLayout>
#include <QAbstractItemView>
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
  layout->setContentsMargins(4, 2, 4, 2);
  layout->setVerticalSpacing(0);

  QFont plain = font();
  plain.setItalic(false);
  QFont italic = plain;
  italic.setItalic(true);
  int row = 0;
  for (const QString& location : db->equipmentSlotOrder()) {
    auto slot = db->equipmentSlotType(location);
    QString suffix;
    QString keyword = db->equipmentSlotType(location).keyword;
    for (int i = 0; i < slot.count; i++) {
      QComboBox* dropdown = new QComboBox(this);
      dropdown->addItem("");
      dropdown->addItem("(ignore)", "ignore");
      dropdown->setItemData(1, italic, Qt::FontRole);
      if (!keyword.isEmpty()) {
        for (const auto& stats : db->searchForItem({{ "worn", keyword }})) {
          dropdown->addItem(stats.uniqueName);
          if (stats.keyword.isEmpty()) {
            dropdown->setItemData(dropdown->count() - 1, italic, Qt::FontRole);
          } else {
            dropdown->setItemData(dropdown->count() - 1, plain, Qt::FontRole);
          }
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
      QObject::connect(dropdown, SIGNAL(currentIndexChanged(int)), this, SLOT(onItemChanged()));
      QObject::connect(statButton, &QToolButton::clicked, [this, slotName]{ showMenu(slotName); });
    }
  }

  layout->setColumnStretch(0, 0);
  layout->setColumnStretch(1, 1);
  layout->setColumnStretch(2, 0);
  layout->setRowStretch(row, 1);
}

void EquipmentView::setItems(const ItemDatabase::EquipmentSet& equipment, bool ignoreEmpty)
{
  int index = 1;
  QString lastSlot;
  for (QComboBox* dropdown : slotItems) {
    dropdown->setCurrentIndex(ignoreEmpty ? 1 : -1);
  }
  QFont plain = font();
  plain.setItalic(false);
  QFont bold = plain;
  bold.setBold(true);
  QFont italic = font();
  italic.setItalic(true);
  QFont boldItalic = bold;
  boldItalic.setItalic(true);
  for (QComboBox* dropdown : slotItems) {
    int index = dropdown->findData(bold, Qt::FontRole);
    if (index >= 0) {
      dropdown->setItemData(index, plain, Qt::FontRole);
    }
    index = dropdown->findData(boldItalic, Qt::FontRole);
    if (index >= 0) {
      dropdown->setItemData(index, italic, Qt::FontRole);
    }
  }
  for (const ItemDatabase::EquipSlot& item : equipment) {
    if (item.location == "_container") {
      continue;
    }

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
    if (db->itemKeyword(item.displayName).isEmpty()) {
      dropdown->setItemData(dropdown->currentIndex(), QVariant::fromValue(boldItalic), Qt::FontRole);
    } else {
      dropdown->setItemData(dropdown->currentIndex(), QVariant::fromValue(bold), Qt::FontRole);
    }
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
    promptForKeyword(currentItem, itemKeyword);
    QComboBox* dropdown = slotItems.value(slotName);
    onItemChanged(dropdown);
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

void EquipmentView::showStats(const QString& _name)
{
  if (_name.isEmpty()) {
    return;
  }
  QString name = _name;
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

ItemDatabase::EquipmentSet EquipmentView::items() const
{
  ItemDatabase::EquipmentSet result;
  for (auto [location, dropdown] : cpairs(slotItems)) {
    if (dropdown->currentData() == "ignore") {
      continue;
    }
    ItemDatabase::EquipSlot slot;
    slot.location = location;
    slot.displayName = dropdown->currentText();
    result << slot;
  }
  return result;
}

void EquipmentView::promptForKeyword(const QString& name, const QString& current)
{
  bool ok = false;
  QString newKeyword = QInputDialog::getText(
    this,
    "Set Keyword",
    QStringLiteral("Keyword for %1:").arg(name),
    QLineEdit::Normal,
    current,
    &ok
  );
  if (ok) {
    db->setItemKeyword(name, newKeyword);
  }
}

void EquipmentView::onItemChanged(QComboBox* dropdown)
{
  if (!dropdown) {
    dropdown = qobject_cast<QComboBox*>(sender());
    if (!dropdown) {
      return;
    }
  }
  QString itemKeyword = db->itemKeyword(dropdown->currentText());
  QFont f(font());
  f.setItalic(itemKeyword.isEmpty());
  dropdown->setFont(f);
}
