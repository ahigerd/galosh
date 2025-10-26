#include "triggertab.h"
#include <QGridLayout>
#include <QFormLayout>
#include <QBoxLayout>
#include <QTreeWidget>
#include <QGroupBox>
#include <QPushButton>
#include <QLineEdit>

TriggerTab::TriggerTab(QWidget* parent)
: QWidget(parent)
{
  QGridLayout* layout = new QGridLayout(this);

  list = new QTreeWidget(this);
  list->setRootIsDecorated(false);
  list->setItemsExpandable(false);
  list->setAllColumnsShowFocus(true);
  list->setIndentation(0);
  list->setUniformRowHeights(true);
  list->setColumnCount(2);
  list->setHeaderLabels({ "Trigger", "Command" });
  QObject::connect(list, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)), this, SLOT(selectTrigger(QTreeWidgetItem*)));
  layout->addWidget(list, 0, 0, 2, 1);

  QGroupBox* bForm = new QGroupBox("Trigger", this);
  QFormLayout* lForm = new QFormLayout(bForm);
  bForm->setMinimumWidth(250);
  layout->addWidget(bForm, 0, 1);

  lForm->addRow("&Pattern:", pattern = new QLineEdit(bForm));
  QObject::connect(pattern, SIGNAL(textEdited(QString)), this, SLOT(updateTrigger()));

  lForm->addRow("&Command:", command = new QLineEdit(bForm));
  QObject::connect(command, SIGNAL(textEdited(QString)), this, SLOT(updateTrigger()));

  QHBoxLayout* lButtons = new QHBoxLayout;
  lButtons->addStretch(1);
  layout->addLayout(lButtons, 1, 1);

  QPushButton* bNew = new QPushButton("&New Trigger", this);
  QObject::connect(bNew, SIGNAL(clicked()), this, SLOT(newTrigger()));
  lButtons->addWidget(bNew, 0);

  bDelete = new QPushButton("&Delete Trigger", this);
  QObject::connect(bDelete, SIGNAL(clicked()), this, SLOT(deleteTrigger()));
  lButtons->addWidget(bDelete, 0);

  selectTrigger(nullptr);
}

void TriggerTab::load(const QString& profile)
{
  manager.loadProfile(profile);
  list->clear();
  for (const TriggerDefinition& def : manager.triggers) {
    if (def.isInternal()) {
      continue;
    }
    addItem(def);
  }
}

bool TriggerTab::save(const QString& profile)
{
  manager.saveProfile(profile);
  return true;
}

void TriggerTab::selectTrigger(QTreeWidgetItem* item)
{
  bDelete->setEnabled(!!item);
  pattern->setEnabled(!!item);
  command->setEnabled(!!item);
  if (!item) {
    pattern->clear();
    command->clear();
    return;
  }
  pattern->setText(item->data(0, Qt::DisplayRole).toString());
  command->setText(item->data(1, Qt::DisplayRole).toString());
}

void TriggerTab::newTrigger()
{
  int nextId = 0;
  for (const auto& def : manager.triggers) {
    bool ok = false;
    int id = def.id.toInt(&ok);
    if (ok && id >= nextId) {
      nextId = id + 1;
    }
  }
  TriggerDefinition def(QString::number(nextId));
  manager.triggers << def;
  QTreeWidgetItem* item = addItem(def);
  list->setCurrentItem(item);
  selectTrigger(item);
  pattern->setFocus();
  markDirty();
}

void TriggerTab::deleteTrigger()
{
  QTreeWidgetItem* item = list->currentItem();
  if (!item) {
    return;
  }

  QString id = item->data(0, Qt::UserRole).toString();
  for (int i = 0; i < manager.triggers.length(); i++) {
    const TriggerDefinition& def = manager.triggers[i];
    if (def.id == id) {
      manager.triggers.removeAt(i);
      delete item;
      break;
    }
  }

  selectTrigger(nullptr);
  markDirty();
}

QTreeWidgetItem* TriggerTab::addItem(const TriggerDefinition& def)
{
  QTreeWidgetItem* item = new QTreeWidgetItem;
  item->setData(0, Qt::UserRole, def.id);
  item->setData(0, Qt::DisplayRole, def.pattern.pattern());
  item->setData(1, Qt::DisplayRole, def.command);
  list->addTopLevelItem(item);
  return item;
}

void TriggerTab::updateTrigger()
{
  QTreeWidgetItem* item = list->currentItem();
  if (!item) {
    return;
  }
  QString id = item->data(0, Qt::UserRole).toString();
  TriggerDefinition* def = manager.findTrigger(id);
  if (!def) {
    return;
  }
  item->setData(0, Qt::DisplayRole, pattern->text());
  item->setData(1, Qt::DisplayRole, command->text());
  def->pattern.setPattern(pattern->text());
  def->command = command->text();
  markDirty();
}
