#include "waypointstab.h"
#include "mapmanager.h"
#include "userprofile.h"
#include "serverprofile.h"
#include "algorithms.h"
#include <QSettings>
#include <QBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QSet>
#include <algorithm>
#include <QtDebug>

WaypointsTab::WaypointsTab(QWidget* parent)
: QWidget(parent)
{
  QVBoxLayout* layout = new QVBoxLayout(this);

  serverProfile = new QLabel(this);
  layout->addWidget(serverProfile);

  table = new QTableWidget(0, 3, this);
  table->setHorizontalHeaderLabels({ "Waypoint", "Room ID", "Description" });
  table->verticalHeader()->setVisible(false);
  table->horizontalHeader()->setStretchLastSection(true);
  table->setSelectionBehavior(QTableWidget::SelectRows);
  QObject::connect(table, SIGNAL(cellChanged(int,int)), this, SLOT(onCellChanged(int,int)));
  layout->addWidget(table, 1);

  QHBoxLayout* buttons = new QHBoxLayout;
  buttons->addStretch(1);
  layout->addLayout(buttons, 0);

  QPushButton* addButton = new QPushButton("&Add", this);
  QObject::connect(addButton, SIGNAL(clicked()), this, SLOT(addRow()));
  buttons->addWidget(addButton);

  QPushButton* delButton = new QPushButton("&Delete", this);
  QObject::connect(delButton, SIGNAL(clicked()), this, SLOT(removeRows()));
  buttons->addWidget(delButton);
}

void WaypointsTab::load(const QString& profile)
{
  UserProfile user(profile);
  mapFile = MapManager::mapForProfile(profile);
  QSettings settings(mapFile, QSettings::IniFormat);

  serverProfile->setText(QStringLiteral("<html>Waypoints for <b>%1</b>:</html>").arg(user.serverProfile->host));

  settings.beginGroup(" Waypoints");
  table->clearContents();
  table->setRowCount(0);
  table->blockSignals(true);
  QMap<int, QTableWidgetItem*> rowMap;
  for (const QString& key : settings.childKeys()) {
    int roomId = settings.value(key).toInt();
    int row = table->rowCount();
    table->insertRow(row);

    QTableWidgetItem* keyItem = new QTableWidgetItem(key);
    keyItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);

    QTableWidgetItem* roomItem = new QTableWidgetItem(QString::number(roomId));
    roomItem->setData(Qt::UserRole, roomId);
    roomItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);

    QTableWidgetItem* descItem = new QTableWidgetItem("[invalid]");
    descItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    rowMap[roomId] = descItem;

    table->setItem(row, 0, keyItem);
    table->setItem(row, 1, roomItem);
    table->setItem(row, 2, descItem);
  }
  settings.endGroup();

  for (const QString& zone : settings.childGroups()) {
    for (auto [roomId, item] : pairs(rowMap)) {
      QString name = settings.value(QStringLiteral("%1/%2/name").arg(zone).arg(roomId)).toString();
      if (!name.isEmpty()) {
        if (!zone.isEmpty() && zone != "-") {
          name = zone + ": " + name;
        }
        item->setData(Qt::DisplayRole, name);
      }
    }
  }
  table->blockSignals(false);
  table->resizeColumnsToContents();
}

bool WaypointsTab::save(const QString& profile)
{
  QSettings settings(MapManager::mapForProfile(profile), QSettings::IniFormat);
  settings.beginGroup(" Waypoints");

  QSet<QString> toRemove;
  for (const QString& key : settings.childKeys()) {
    toRemove << key;
  }

  QSet<QString> seen;
  QMap<QString, int> toSet;
  for (int i = 0; i < table->rowCount(); i++) {
    QString name = table->item(i, 0)->data(Qt::DisplayRole).toString();
    int roomId = table->item(i, 1)->data(Qt::UserRole).toInt();

    if (name.isEmpty()) {
      table->setCurrentCell(i, 0, QItemSelectionModel::ClearAndSelect);
      QMessageBox::critical(this, "Galosh", "Waypoint name is required.");
      return false;
    } else if (seen.contains(name)) {
      table->setCurrentCell(i, 0, QItemSelectionModel::ClearAndSelect);
      QMessageBox::critical(this, "Galosh", "Waypoint name already exists.");
      return false;
    }
    seen << name;
    toRemove.remove(name);

    if (roomId <= 0) {
      table->setCurrentCell(i, 1, QItemSelectionModel::ClearAndSelect);
      QMessageBox::critical(this, "Galosh", "Waypoint room ID is invalid.");
      return false;
    }
    toSet[name] = roomId;
  }

  for (const QString& key : toRemove) {
    settings.remove(key);
  }
  for (auto [key, roomId] : pairs(toSet)) {
    settings.setValue(key, roomId);
  }
  return true;
}

void WaypointsTab::onCellChanged(int row, int col)
{
  if (row < 0 || row >= table->rowCount()) {
    return;
  }
  if (col == 0) {
    emit markDirty();
  } else if (col == 1) {
    table->blockSignals(true);
    int roomId = table->item(row, col)->data(Qt::EditRole).toInt();
    table->item(row, 1)->setData(Qt::UserRole, -1);
    table->item(row, 2)->setData(Qt::DisplayRole, "[invalid]");
    if (roomId > 0) {
      QSettings settings(mapFile, QSettings::IniFormat);
      for (const QString& zone : settings.childGroups()) {
        QString name = settings.value(QStringLiteral("%1/%2/name").arg(zone).arg(roomId)).toString();
        if (!name.isEmpty()) {
          if (!zone.isEmpty() && zone != "-") {
            name = zone + ": " + name;
          }
          table->item(row, 1)->setData(Qt::UserRole, roomId);
          table->item(row, 2)->setData(Qt::DisplayRole, name);
          break;
        }
      }
    }
    table->blockSignals(false);
    emit markDirty();
  }
}

void WaypointsTab::addRow()
{
  table->blockSignals(true);
  int row = table->rowCount();
  table->insertRow(row);

  QTableWidgetItem* keyItem = new QTableWidgetItem("");
  keyItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);

  QTableWidgetItem* roomItem = new QTableWidgetItem("");
  roomItem->setData(Qt::UserRole, -1);
  roomItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);

  QTableWidgetItem* descItem = new QTableWidgetItem("[invalid]");
  descItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

  table->setItem(row, 0, keyItem);
  table->setItem(row, 1, roomItem);
  table->setItem(row, 2, descItem);
  table->blockSignals(false);

  table->setCurrentItem(keyItem, QItemSelectionModel::ClearAndSelect);
  table->editItem(keyItem);

  emit markDirty();
}

void WaypointsTab::removeRows()
{
  QSet<int> rows;
  for (QTableWidgetItem* item : table->selectedItems()) {
    rows << item->row();
  }
  QList<int> sorted = rows.values();
  std::sort(sorted.rbegin(), sorted.rend());
  for (int row : sorted) {
    table->removeRow(row);
  }
  emit markDirty();
}
