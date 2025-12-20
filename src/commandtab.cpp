#include "commandtab.h"
#include "userprofile.h"
#include "algorithms.h"
#include <QMessageBox>
#include <QBoxLayout>
#include <QGridLayout>
#include <QListWidget>
#include <QScrollArea>
#include <QLineEdit>
#include <QToolButton>
#include <QLabel>
#include <QtDebug>

CommandTab::CommandTab(QWidget* parent)
: DialogTabBase(tr("Commands"), parent)
{
  QGridLayout* layout = new QGridLayout(this);

  QVBoxLayout* lLeft = new QVBoxLayout;
  layout->addLayout(lLeft, 0, 0, 4, 1);

  lCommands = new QListWidget(this);
  QObject::connect(lCommands, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)), this, SLOT(onCommandSelected(QListWidgetItem*)));
  lLeft->addWidget(lCommands, 1);

  QHBoxLayout* lButtons = new QHBoxLayout;
  lButtons->addStretch(1);
  lLeft->addLayout(lButtons, 0);

  QToolButton* bAdd = new QToolButton(this);
  bAdd->setIcon(QIcon::fromTheme("document-new"));
  QObject::connect(bAdd, SIGNAL(clicked()), this, SLOT(addCommand()));
  lButtons->addWidget(bAdd, 0);

  QToolButton* bDelete = new QToolButton(this);
  bDelete->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
  QObject::connect(bDelete, SIGNAL(clicked()), this, SLOT(removeCommands()));
  lButtons->addWidget(bDelete, 0);

  QLabel* lCommand = new QLabel("Co&mmand:", this);
  layout->addWidget(lCommand, 0, 1);

  eCommand = new QLineEdit(this);
  lCommand->setBuddy(eCommand);
  QObject::connect(eCommand, SIGNAL(textEdited(QString)), this, SLOT(onCommandChanged()));
  layout->addWidget(eCommand, 0, 2);

  layout->addWidget(horizontalLine(this), 1, 1, 1, 2);

  QLabel* lblActions = new QLabel("&Actions:", this);
  layout->addWidget(lblActions, 2, 1, 1, 2);

  QScrollArea* saActions = new QScrollArea(this);
  lblActions->setBuddy(saActions);
  layout->addWidget(saActions, 3, 1, 1, 2);

  QWidget* wActions = new QWidget(saActions);
  lActions = new QVBoxLayout(wActions);
  lActions->addStretch(1);
  saActions->setWidget(wActions);
  saActions->setWidgetResizable(true);

  layout->setRowStretch(3, 1);
  layout->setColumnStretch(0, 1);
  layout->setColumnStretch(2, 2);
}

void CommandTab::load(UserProfile* profile)
{
  lCommands->clear();
  clearCommand();
  for (const QString& command : profile->customCommands()) {
    QListWidgetItem* item = new QListWidgetItem(command, lCommands);
    item->setData(Qt::UserRole, profile->customCommand(command));
  }
  lCommands->setCurrentRow(0);
}

bool CommandTab::save(UserProfile* profile)
{
  int count = lCommands->count();
  QStringList toRemove = profile->customCommands();
  for (int i = 0; i < count; i++) {
    QListWidgetItem* item = lCommands->item(i);
    QString command = item->text();
    QStringList actions = item->data(Qt::UserRole).toStringList();
    if (actions.isEmpty() || (actions.count() == 1 && actions[0].isEmpty())) {
      lCommands->setCurrentItem(item);
      QMessageBox::critical(this, "Galosh", "Custom commands must have at least one action.");
      return false;
    }
    toRemove.removeAll(command);
    profile->setCustomCommand(command, actions);
  }
  for (const QString& command : toRemove) {
    profile->setCustomCommand(command, {});
  }
  return true;
}

void CommandTab::addCommand()
{
  QListWidgetItem* item = new QListWidgetItem(lCommands);
  lCommands->setCurrentItem(item);
  clearCommand();
  onCommandSelected(lCommands->currentItem());
  emit markDirty();
}

void CommandTab::clearCommand()
{
  eCommand->clear();
  for (auto [line, remove] : rows) {
    lActions->removeWidget(line);
    line->deleteLater();
    lActions->removeWidget(remove);
    remove->deleteLater();
  }
  rows.clear();
}

void CommandTab::removeCommands()
{
  auto items = lCommands->selectedItems();
  QListWidgetItem* current = lCommands->currentItem();
  if (items.isEmpty()) {
    if (current) {
      items << current;
    } else {
      return;
    }
  }
  int firstRow = lCommands->row(items.first());
  for (QListWidgetItem* item : items) {
    delete item;
  }
  lCommands->setCurrentRow(firstRow);
  onCommandSelected(lCommands->currentItem());
  emit markDirty();
}

void CommandTab::onCommandSelected(QListWidgetItem* item)
{
  clearCommand();
  if (item) {
    eCommand->setText(item->text());
    for (const QString& action : item->data(Qt::UserRole).toStringList()) {
      addAction();
      rows.last().line->setText(action);
    }
    addAction();
    eCommand->setFocus();
  }
}

void CommandTab::addAction()
{
  QHBoxLayout* row = new QHBoxLayout;

  QLineEdit* line = new QLineEdit;
  QObject::connect(line, SIGNAL(textEdited(QString)), this, SLOT(onActionChanged()));
  row->addWidget(line, 1);

  QToolButton* remove = new QToolButton;
  remove->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
  QObject::connect(remove, SIGNAL(clicked()), this, SLOT(removeAction()));
  row->addWidget(remove, 0);

  lActions->insertLayout(lActions->count() - 1, row, 0);
  rows << (Row){ line, remove };

  emit markDirty();
}

void CommandTab::removeAction()
{
  QToolButton* clicked = qobject_cast<QToolButton*>(sender());
  if (!clicked) {
    return;
  }
  for (auto [index, row] : enumerate(rows)) {
    if (row.remove == clicked) {
      row.line->deleteLater();
      row.remove->deleteLater();
      rows.removeAt(index);
      break;
    }
  }
  onActionChanged();
  emit markDirty();
}

void CommandTab::onCommandChanged()
{
  QListWidgetItem* item = lCommands->currentItem();
  if (!item) {
    item = new QListWidgetItem(lCommands);
    lCommands->setCurrentItem(item);
    addAction();
  }
  item->setText(eCommand->text());
}

void CommandTab::onActionChanged()
{
  QListWidgetItem* item = lCommands->currentItem();
  if (!item) {
    return;
  }
  QStringList actions;
  bool lastIsEmpty = false;
  for (auto [line, _] : rows) {
    QString action = line->text();
    lastIsEmpty = action.isEmpty();
    if (!lastIsEmpty) {
      actions << action;
    }
  }
  item->setData(Qt::UserRole, actions);
  if (!lastIsEmpty) {
    addAction();
  }
  emit markDirty();
}
