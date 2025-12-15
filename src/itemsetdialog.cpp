#include "itemsetdialog.h"
#include "userprofile.h"
#include "serverprofile.h"
#include "itemdatabase.h"
#include "equipmentview.h"
#include "settingsgroup.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QToolButton>
#include <QMessageBox>
#include <QStyle>
#include <QMenu>
#include <QInputDialog>
#include <QtDebug>

ItemSetDialog::ItemSetDialog(UserProfile* profile, bool newSet, QWidget* parent)
: QDialog(parent), profile(profile), db(&profile->serverProfile->itemDB), equipButton(nullptr), newSetMode(newSet)
{
  if (newSet) {
    setWindowTitle("New Item Set");
  } else {
    setWindowTitle("Item Sets");
  }

  QFormLayout* layout = new QFormLayout(this);
  layout->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);

  if (!newSet) {
    QHBoxLayout* lSet = new QHBoxLayout;
    lSet->setContentsMargins(0, 0, 0, 0);

    cSetName = new QComboBox(this);
    lSet->addWidget(cSetName, 1);

    QToolButton* bSet = new QToolButton(this);
    bSet->setText("...");
    QObject::connect(bSet, SIGNAL(clicked()), this, SLOT(setMenu()));
    lSet->addWidget(bSet, 0);

    layout->addRow("Item set:", lSet);
    QObject::connect(cSetName, SIGNAL(currentTextChanged(QString)), this, SLOT(loadNamedSet(QString)));
  }

  QScrollArea* saEquip = new QScrollArea(this);
  saEquip->setWidgetResizable(true);
  layout->addRow(saEquip);

  equipView = new EquipmentView(db, this);
  saEquip->setWidget(equipView);
  int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
  int scrollWidth = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
  saEquip->setMinimumWidth(equipView->minimumSizeHint().width() + frameWidth * 2 + scrollWidth);

  if (newSet) {
    cContainer = nullptr;
    cSetName = new QComboBox(this);
    cSetName->setEditable(true);
    layout->addRow("Save set as:", cSetName);
  } else {
    QHBoxLayout* lContainer = new QHBoxLayout;
    lContainer->setContentsMargins(0, 0, 0, 0);

    cContainer = new QComboBox(this);
    cContainer->addItem("inventory");
    QFont italic = font();
    italic.setItalic(true);
    cContainer->setItemData(0, QVariant::fromValue(italic), Qt::FontRole);
    lContainer->addWidget(cContainer, 1);

    QToolButton* bContainer = new QToolButton(this);
    bContainer->setText("...");
    QObject::connect(bContainer, SIGNAL(clicked()), this, SLOT(containerMenu()));
    lContainer->addWidget(bContainer, 0);

    QList<ItemQuery> queries;
    queries << (ItemQuery){ "type", QStringList({"CONTAINER"}) };
    for (const ItemStats& container : db->searchForItem(queries)) {
      if (container.keyword.isEmpty()) {
        cContainer->addItem(container.name, container.name);
      } else {
        cContainer->addItem(QStringLiteral("%1 (%2)").arg(container.name).arg(container.keyword), container.name);
      }
    }

    layout->addRow("Store items in:", lContainer);
  }

  cSetName->setInsertPolicy(QComboBox::NoInsert);
  cSetName->addItems(profile->itemSets());
  if (newSet) {
    cSetName->setCurrentText("");
    cSetName->setCurrentIndex(-1);
  }

  QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Apply | QDialogButtonBox::Close | QDialogButtonBox::Reset, this);
  if (!newSet) {
    equipButton = buttons->addButton("&Equip Now", QDialogButtonBox::ActionRole);
    QObject::connect(equipButton, SIGNAL(clicked()), this, SLOT(equipCurrentSet()));
  }
  QObject::connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
  QObject::connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
  QObject::connect(buttons->button(QDialogButtonBox::Reset), SIGNAL(clicked()), this, SLOT(reset()));
  QObject::connect(buttons->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(save()));
  layout->addRow(buttons);
}

void ItemSetDialog::loadNewEquipment(const ItemDatabase::EquipmentSet& equipment)
{
  loadedEquipment = equipment;
  equipView->setItems(equipment);
  if (cContainer) {
    cContainer->setCurrentIndex(0);
  }
  loadedContainer.clear();
}

void ItemSetDialog::loadNamedSet(const QString& setName)
{
  loadedEquipment = profile->loadItemSet(setName);
  cSetName->setCurrentText(setName);
  equipView->setItems(loadedEquipment, true);
  if (cContainer) {
    cContainer->setCurrentIndex(0);
  }
  for (auto [slot, name] : loadedEquipment) {
    if (slot == "_container") {
      loadedContainer = name;
      if (cContainer) {
        cContainer->setCurrentIndex(cContainer->findData(name));
      }
      break;
    }
  }
}

void ItemSetDialog::reset()
{
  equipView->setItems(loadedEquipment, !newSetMode);
  cSetName->setCurrentText("");
  if (cContainer) {
    if (loadedContainer.isEmpty()) {
      cContainer->setCurrentIndex(0);
    } else {
      cContainer->setCurrentText(loadedContainer);
    }
  }
}

void ItemSetDialog::setConnected(bool isConnected)
{
  if (equipButton) {
    equipButton->setEnabled(isConnected);
  }
}

void ItemSetDialog::done(int r)
{
  if (r == QDialog::Accepted) {
    if (!save()) {
      return;
    }
  }
  QDialog::done(r);
}

bool ItemSetDialog::validateSetName(const QString& name)
{
  if (name.isEmpty()) {
    QMessageBox::critical(this, "Galosh", "Item set name is required.");
    return false;
  } else if (name.contains(" ")) {
    QMessageBox::critical(this, "Galosh", "Item set name should not contain spaces.");
    return false;
  } else {
    return true;
  }
}

bool ItemSetDialog::save()
{
  QString name = cSetName->currentText();
  if (!validateSetName(name)) {
    return false;
  }
  if (newSetMode && cSetName->findText(name) >= 0) {
    int response = QMessageBox::question(this, "Galosh", QStringLiteral("Do you want to replace the item set \"%1\"?").arg(name), QMessageBox::Ok | QMessageBox::Cancel);
    if (response != QMessageBox::Ok) {
      return false;
    }
  }
  ItemDatabase::EquipmentSet set = equipView->items();
  if (cContainer && cContainer->currentIndex() > 0) {
    QString container = cContainer->currentData().toString();
    set << (ItemDatabase::EquipSlot){ "_container", container };
  }
  profile->saveItemSet(name, set);
  loadNamedSet(name);
  return true;
}

void ItemSetDialog::equipCurrentSet()
{
  if (!save()) {
    return;
  }
  emit equipNamedSet(cSetName->currentText());
}

void ItemSetDialog::setMenu()
{
  QMenu menu;

  QAction* newAction = menu.addAction("Create &new set...");
  QAction* renameAction = menu.addAction("&Rename set...");
  renameAction->setEnabled(cSetName->currentIndex() >= 0);
  QAction* deleteAction = menu.addAction("&Delete set...");

  QAction* action = menu.exec(QCursor::pos());
  if (action == newAction || action == renameAction) {
    bool rename = (action == renameAction);
    bool ok = false;
    QString newName = QInputDialog::getText(
      this,
      rename ? "Rename Item Set" : "Create New Item Set",
      "Name for item set:",
      QLineEdit::Normal,
      rename ? cSetName->currentText() : "",
      &ok
    );
    if (ok && validateSetName(newName)) {
      if (rename) {
        profile->removeItemSet(cSetName->currentText());
        cSetName->setItemText(cSetName->currentIndex(), newName);
      } else {
        if (cSetName->findText(newName) >= 0) {
          QMessageBox::critical(this, "Galosh", QStringLiteral("An item set with the name \"%1\" already exists.").arg(newName));
          return;
        }
        cSetName->addItem(newName);
      }
      cSetName->setCurrentText(newName);
      save();
    }
  } else if (action == deleteAction) {
    profile->removeItemSet(cSetName->currentText());
    cSetName->removeItem(cSetName->currentIndex());
  }
}

void ItemSetDialog::containerMenu()
{
  QString currentItem = cContainer->currentData().toString();
  QString itemKeyword = db->itemKeyword(currentItem);
  bool isInventory = (cContainer->currentIndex() == 0);

  QMenu menu;
  menu.addSection(currentItem);

  QAction* detailsAction = menu.addAction("Sh&ow Details...");
  if (isInventory) {
    detailsAction->setEnabled(false);
  }

  QAction* keywordAction = menu.addAction("Set &Keyword...");
  if (isInventory) {
    keywordAction->setEnabled(false);
  } else {
    if (!itemKeyword.isEmpty()) {
      keywordAction->setText(QStringLiteral("&Keyword: %1").arg(itemKeyword.replace("&", "&&")));
    }
  }

  QAction* action = menu.exec(QCursor::pos());
  if (action == detailsAction) {
    equipView->showStats(currentItem);
  } else if (action == keywordAction) {
    equipView->promptForKeyword(currentItem, itemKeyword);
  }
}
