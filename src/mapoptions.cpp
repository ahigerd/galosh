#include "mapoptions.h"
#include "mapmanager.h"
#include <QSettings>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QMessageBox>
#include <QColorDialog>
#include <QtDebug>

MapOptions::MapOptions(MapManager* map, QWidget* parent)
: QDialog(parent), map(map), isDirty(false)
{
  setWindowTitle("Map Settings");

  QVBoxLayout* layout = new QVBoxLayout(this);

  QTabWidget* tabs = new QTabWidget(this);
  layout->addWidget(tabs, 1);

  QWidget* tColor = new QWidget(tabs);
  QVBoxLayout* lColor = new QVBoxLayout(tColor);

  table = new QTableWidget(0, 3, this);
  table->setHorizontalHeaderLabels({ "Room Type", "Cost", "Color" });
  table->verticalHeader()->setVisible(false);
  table->horizontalHeader()->setStretchLastSection(true);
  table->setSelectionBehavior(QTableWidget::SelectRows);
  lColor->addWidget(table, 1);

  QHBoxLayout* lButtons = new QHBoxLayout;
  lButtons->addStretch(1);
  lColor->addLayout(lButtons, 0);

  QPushButton* addButton = new QPushButton("&Add", this);
  QObject::connect(addButton, SIGNAL(clicked()), this, SLOT(addRow()));
  lButtons->addWidget(addButton);

  QPushButton* delButton = new QPushButton("&Delete", this);
  QObject::connect(delButton, SIGNAL(clicked()), this, SLOT(removeRows()));
  lButtons->addWidget(delButton);

  tabs->addTab(tColor, "&Colors");

  QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
  layout->addWidget(buttons, 0);

  for (const QString& roomType : map->roomTypes()) {
    int row = table->rowCount();
    table->insertRow(row);

    QTableWidgetItem* nameItem = new QTableWidgetItem(roomType);
    nameItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);

    QTableWidgetItem* costItem = new QTableWidgetItem(QString::number(map->roomCost(roomType)));
    costItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
    costItem->setData(Qt::TextAlignmentRole, int(Qt::AlignRight | Qt::AlignVCenter));

    QTableWidgetItem* colorItem = new QTableWidgetItem;
    colorItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    QColor color = map->roomColor(roomType);
    colorItem->setData(Qt::DecorationRole, color.isValid() ? color : QColor(Qt::white));
    colorItem->setData(Qt::DisplayRole, color.isValid() ? color.name() : "");

    table->setItem(row, 0, nameItem);
    table->setItem(row, 1, costItem);
    table->setItem(row, 2, colorItem);
  }

  table->resizeColumnsToContents();

  QObject::connect(table, SIGNAL(cellChanged(int,int)), this, SLOT(onCellChanged(int,int)));
  QObject::connect(table, SIGNAL(itemDoubleClicked(QTableWidgetItem*)), this, SLOT(onItemDoubleClicked(QTableWidgetItem*)));
  QObject::connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
  QObject::connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
  QObject::connect(buttons->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(save()));
}

bool MapOptions::save()
{
  for (int i = 0; i < table->rowCount(); i++) {
    QTableWidgetItem* nameItem = table->item(i, 0);
    if (nameItem->data(Qt::DisplayRole).toString().isEmpty()) {
      table->setCurrentCell(i, 0, QItemSelectionModel::ClearAndSelect);
      QMessageBox::critical(this, "Galosh", "Room type is required.");
      return false;
    }
    QTableWidgetItem* costItem = table->item(i, 1);
    if (costItem->data(Qt::DisplayRole).toInt() < 1) {
      table->setCurrentCell(i, 1, QItemSelectionModel::ClearAndSelect);
      QMessageBox::critical(this, "Galosh", "Cost must be at least 1.");
      return false;
    }
  }

  QSettings* profile = map->mapProfile();
  if (!profile) {
    return false;
  }

  QStringList toDelete = map->roomTypes();
  for (int i = 0; i < table->rowCount(); i++) {
    QTableWidgetItem* nameItem = table->item(i, 0);
    QTableWidgetItem* costItem = table->item(i, 1);
    QTableWidgetItem* colorItem = table->item(i, 2);

    QString name = nameItem->data(Qt::DisplayRole).toString();
    toDelete.removeAll(name);

    int cost = costItem->data(Qt::DisplayRole).toInt();

    QColor color;
    if (!colorItem->data(Qt::DisplayRole).toString().isEmpty()) {
      color = colorItem->data(Qt::DecorationRole).value<QColor>();
    }

    map->setRoomCost(name, cost);
    map->setRoomColor(name, color);
  }

  for (const QString& roomType : toDelete) {
    map->removeRoomType(roomType);
  }
  return true;
}

void MapOptions::done(int r)
{
  if (r == QDialog::Accepted) {
    if (!save()) {
      return;
    }
  } else if (isDirty) {
    auto button = QMessageBox::question(nullptr, "Galosh", "There are unsaved changes. Close anyway?", QMessageBox::Discard | QMessageBox::Save | QMessageBox::Cancel);
    if (button == QMessageBox::Save) {
      if (!save()) {
        return;
      }
    } else if (button == QMessageBox::Cancel) {
      return;
    }
  }
  QDialog::done(r);
  deleteLater();
}

void MapOptions::addRow()
{
  table->blockSignals(true);

  int row = table->rowCount();
  table->insertRow(row);

  QTableWidgetItem* nameItem = new QTableWidgetItem("");
  nameItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);

  QTableWidgetItem* costItem = new QTableWidgetItem("1");
  costItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
  costItem->setData(Qt::TextAlignmentRole, int(Qt::AlignRight | Qt::AlignVCenter));

  QTableWidgetItem* colorItem = new QTableWidgetItem;
  colorItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
  colorItem->setData(Qt::DecorationRole, QColor(Qt::white));
  colorItem->setData(Qt::DisplayRole, "");

  table->setItem(row, 0, nameItem);
  table->setItem(row, 1, costItem);
  table->setItem(row, 2, colorItem);

  table->setCurrentItem(nameItem, QItemSelectionModel::ClearAndSelect);
  table->editItem(nameItem);

  table->blockSignals(false);
  isDirty = true;
}

void MapOptions::removeRows()
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
  isDirty = true;
}

void MapOptions::onCellChanged(int, int)
{
  isDirty = true;
}

void MapOptions::onItemDoubleClicked(QTableWidgetItem* item)
{
  if (!item || item->column() != 2) {
    return;
  }
  QColorDialog dlg(item->data(Qt::DecorationRole).value<QColor>(), this);
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }
  QColor color = dlg.selectedColor();
  item->setData(Qt::DecorationRole, color.isValid() ? color : Qt::white);
  item->setData(Qt::DisplayRole, color.isValid() ? color.name() : "");
  isDirty = true;
}
