#include "roomview.h"
#include "mapmanager.h"
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QBoxLayout>
#include <QtDebug>

RoomView::RoomView(QWidget* parent)
: QWidget(parent)
{
  QWidget* content = this; // new QWidget(this);
  QHBoxLayout* layout = new QHBoxLayout(content);
  QMargins margins = layout->contentsMargins();
  layout->setContentsMargins(margins.left(), 0, margins.right(), margins.bottom());

  roomDesc = new QLabel(content);
  roomDesc->setWordWrap(true);
  roomDesc->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  roomDesc->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  layout->addWidget(roomDesc, 1);

  exitBox = new QGroupBox("Exits", content);
  layout->addWidget(exitBox, 0);

  QVBoxLayout* exitLayout = new QVBoxLayout(exitBox);
  exitLayout->setContentsMargins(0, 0, 0, 0);

  exits = new QListWidget(exitBox);
  exitLayout->addWidget(exits, 1);

  setRoom(nullptr, 0);
  //setWidget(content);
}

QSize RoomView::sizeHint() const
{
  QSize hint = layout()->sizeHint();
  int h = 4 * exits->sizeHintForRow(0);
  if (h <= 0) {
    h = 80;
  }
  h += exitBox->contentsMargins().top() + exitBox->contentsMargins().bottom();
  return QSize(hint.width(), h);
}

void RoomView::setRoom(MapManager* map, int roomId)
{
  const MapRoom* room = map ? map->room(roomId) : nullptr;
  if (room) {
    QString title = QStringLiteral("%3: %1 [%2] (%4)").arg(room->name).arg(room->id).arg(room->zone).arg(room->roomType);
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
      if (exit.dest) {
        destName += QStringLiteral(" [%1]").arg(exit.dest);
      }
      exits->addItem(QStringLiteral("%1: %2%3").arg(dir).arg(destName).arg(status));
    }
    emit roomUpdated(title);
  } else {
    emit roomUpdated("Unknown location");
  }
  if (!exits->count()) {
    QListWidgetItem* blank = new QListWidgetItem;
    blank->setText("(no visible exits)");
    blank->setFlags(Qt::NoItemFlags);
    exits->addItem(blank);
  }
}
