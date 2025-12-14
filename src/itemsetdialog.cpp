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
#include <QMessageBox>
#include <QStyle>
#include <QtDebug>

ItemSetDialog::ItemSetDialog(UserProfile* profile, bool newSet, QWidget* parent)
: QDialog(parent), profile(profile), db(&profile->serverProfile->itemDB), newSetMode(newSet)
{
  QFormLayout* layout = new QFormLayout(this);

  if (!newSet) {
    setName = new QComboBox(this);
    layout->addRow("Item set:", setName);
  }

  QGroupBox* gEquip = new QGroupBox("Equipment", this);
  QVBoxLayout* lEquip = new QVBoxLayout(gEquip);
  lEquip->setContentsMargins(0, 0, 0, 0);
  layout->addRow(gEquip);

  QScrollArea* saEquip = new QScrollArea(gEquip);
  saEquip->setFrameStyle(QFrame::NoFrame);
  saEquip->setWidgetResizable(true);
  lEquip->addWidget(saEquip, 1);

  equipView = new EquipmentView(db, this);
  saEquip->setWidget(equipView);
  int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
  int scrollWidth = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
  saEquip->setMinimumWidth(equipView->minimumSizeHint().width() + frameWidth * 2 + scrollWidth);

  if (newSet) {
    setName = new QComboBox(this);
    setName->setEditable(true);
    layout->addRow("Save set as:", setName);
  }

  setName->setInsertPolicy(QComboBox::NoInsert);
  QSettings settings(profile->profilePath, QSettings::IniFormat);
  for (const QString& group : settings.childGroups()) {
    if (!group.startsWith("ItemSet-")) {
      continue;
    }
    setName->addItem(group.mid(8));
  }
  if (newSet) {
    setName->setCurrentText("");
    setName->setCurrentIndex(-1);
  }

  QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close | QDialogButtonBox::Reset, this);
  if (newSet) {
    buttons->addButton(QDialogButtonBox::Save);
  }
  QObject::connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
  QObject::connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
  QObject::connect(buttons->button(QDialogButtonBox::Reset), SIGNAL(clicked()), this, SLOT(reset()));
  layout->addRow(buttons);
}

void ItemSetDialog::loadNewEquipment(const QList<ItemDatabase::EquipSlot>& equipment)
{
  loadedEquipment = equipment;
  equipView->setItems(equipment);
}

void ItemSetDialog::reset()
{
  equipView->setItems(loadedEquipment);
  setName->setCurrentText("");
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

bool ItemSetDialog::save()
{
  QString name = setName->currentText();
  if (name.isEmpty()) {
    QMessageBox::critical(this, "Galosh", "Item set name is required.");
    return false;
  }
  if (newSetMode && setName->findText(name) >= 0) {
    int response = QMessageBox::question(this, "Galosh", QStringLiteral("Do you want to replace the item set \"%1\"?").arg(name), QMessageBox::Ok | QMessageBox::Cancel);
    if (response != QMessageBox::Ok) {
      return false;
    }
  }
  QSettings settings(profile->profilePath, QSettings::IniFormat);
  settings.remove("ItemSet-" + name);
  SettingsGroup sg(&settings, "ItemSet-" + name);
  for (const ItemDatabase::EquipSlot& slot : equipView->items()) {
    settings.setValue(slot.location, slot.displayName);
  }
  return true;
}
