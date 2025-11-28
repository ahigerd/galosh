#include "mapoptions.h"
#include "mapmanager.h"
#include <QBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QtDebug>

MapOptions::MapOptions(MapManager* map, QWidget* parent)
: QDialog(parent), map(map)
{
  QVBoxLayout* layout = new QVBoxLayout(this);

  table = new QTableWidget(0, 3, this);
  table->setHorizontalHeaderLabels({ "Room Type", "Cost", "Color" });
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
    colorItem->setData(Qt::BackgroundRole, QBrush(map->roomColor(roomType)));

    table->setItem(row, 0, nameItem);
    table->setItem(row, 1, costItem);
    table->setItem(row, 2, colorItem);
  }

  table->resizeColumnsToContents();
}

bool MapOptions::save()
{
}

void MapOptions::done(int r)
{
  if (r == QDialog::Accepted) {
    if (!save()) {
      return;
    }
    /*
    if (emitConnect) {
      QModelIndex idx = knownProfiles->currentIndex();
      if (!idx.isValid()) {
        qDebug() << "No profile selected";
      } else {
        QSettings settings;
        QDir dir(idx.data(Qt::UserRole).toString());
        settings.setValue("lastProfile", dir.dirName().replace(".galosh", ""));
        emit connectToProfile(idx.data(Qt::UserRole).toString());
      }
    }
    */
  }
  QDialog::done(r);
  deleteLater();
}
