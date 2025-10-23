#include "roomview.h"
#include "mapmanager.h"
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QBoxLayout>

RoomView::RoomView(QWidget* parent)
: QWidget(parent)
{
  QHBoxLayout* layout = new QHBoxLayout(this);

  roomBox = new QGroupBox("[no room]", this);
  layout->addWidget(roomBox, 1);

  QVBoxLayout* boxLayout = new QVBoxLayout(roomBox);
  boxLayout->setContentsMargins(0, 0, 0, 0);

  roomDesc = new QLabel(this);
  roomDesc->setWordWrap(true);
  roomDesc->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  boxLayout->addWidget(roomDesc, 1);

  QGroupBox* exitBox = new QGroupBox("Exits", this);
  layout->addWidget(exitBox, 0);

  QVBoxLayout* exitLayout = new QVBoxLayout(exitBox);
  exitLayout->setContentsMargins(0, 0, 0, 0);

  exits = new QListWidget(this);
  exitLayout->addWidget(exits, 1);
}

void RoomView::setRoom(MapManager* map, int roomId)
{
  const MapRoom* room = map->room(roomId);
  if (!room) {
    // TODO
    return;
  }
  roomBox->setTitle(room->name);
  roomDesc->setText(room->description.simplified());
  exits->clear();
  for (const QString& dir : room->exits.keys()) {
    MapExit exit = room->exits[dir];
    const MapRoom* dest = map->room(exit.dest);
    QString status;
    if (exit.door) {
      status = QStringLiteral(" (%1: %2)").arg(exit.name);
      if (exit.locked) {
        status = status.arg("locked");
      } else if (exit.open) {
        status = status.arg("open");
      } else {
        status = status.arg("closed");
      }
    }
    QString destName = "[unknown]";
    if (dest && !dest->name.isEmpty()) {
      destName = dest->name;
    }
    exits->addItem(QStringLiteral("%1: %2%3").arg(dir).arg(destName).arg(status));
  }
}
