#include "roomview.h"
#include "mapmanager.h"
#include "exploredialog.h"
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QGridLayout>
#include <QBoxLayout>
#include <QPushButton>
#include <QTextBrowser>
#include <QScrollBar>
#include <QTimer>
#include <QtDebug>

QString RoomView::formatRoomTitle(const MapRoom* room)
{
  if (!room) {
    return "(unknown location)";
  }
  QString title;
  if (!room->zone.isEmpty()) {
    title = room->zone + ": ";
  }
  title += QStringLiteral("%1 [%2]").arg(room->name).arg(room->id);
  if (!room->roomType.isEmpty()) {
    title += QStringLiteral(" (%1)").arg(room->roomType);
  }
  return title;
}

RoomView::RoomView(QWidget* parent)
: QWidget(parent), mapManager(nullptr), currentRoomId(-1), lastRoomId(-1)
{
  QGridLayout* layout = new QGridLayout(this);
  QMargins margins = layout->contentsMargins();
  layout->setContentsMargins(margins.left(), 0, margins.right(), margins.bottom());

  roomDesc = new QLabel(this);
  roomDesc->setWordWrap(true);
  roomDesc->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  roomDesc->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  layout->addWidget(roomDesc, 0, 0);

  responseMessage = new QTextBrowser(this);
  responseMessage->setWordWrapMode(QTextOption::NoWrap);
  responseMessage->setVisible(false);
  layout->addWidget(responseMessage, 1, 0);

  exitBox = new QGroupBox("Exits", this);
  layout->addWidget(exitBox, 0, 1, 2, 1);

  QVBoxLayout* exitLayout = new QVBoxLayout(exitBox);
  exitLayout->setContentsMargins(0, 0, 0, 0);

  exits = new QListWidget(exitBox);
  exitLayout->addWidget(exits, 1);

  layout->setRowStretch(0, 2);
  layout->setRowStretch(1, 0);
  layout->setColumnStretch(0, 1);
  layout->setColumnStretch(1, 0);

  setRoom(nullptr, 0);

  QObject::connect(exits, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(exploreRoom(QListWidgetItem*)));
}

RoomView::RoomView(MapManager* map, int roomId, int lastRoomId, QWidget* parent)
: RoomView(parent)
{
  mapManager = map;
  if (roomId < 0) {
    currentRoomId = -1;
    setRoom(map, lastRoomId);
  } else {
    currentRoomId = lastRoomId;
    setRoom(map, roomId);
  }
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

void RoomView::setRoom(MapManager* map, int roomId, const QString& movement)
{
  if (roomId >= 0) {
    if (map != mapManager) {
      mapManager = map;
      lastRoomId = -1;
    } else {
      lastRoomId = currentRoomId;
    }
    currentRoomId = roomId;
  }
  const MapRoom* room = map ? map->room(roomId) : nullptr;
  if (room) {
    QString title = formatRoomTitle(room);
    roomDesc->setText(room->description.simplified());
    if (roomDesc->text().isEmpty()) {
      roomDesc->setText("(Room description not available)");
    }
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
      QListWidgetItem* item = new QListWidgetItem;
      item->setText(QStringLiteral("%1: %2%3").arg(dir == "SOMEWHERE" ? "?" : dir).arg(destName).arg(status));
      item->setData(Qt::UserRole, exit.dest);
      item->setData(Qt::UserRole + 1, dir);
      exits->addItem(item);
    }
    emit roomUpdated(title, roomId, movement);
  } else {
    emit roomUpdated("Unknown location", -1, movement);
    roomDesc->setText("(description unavailable)");
  }
  if (!exits->count()) {
    QListWidgetItem* blank = new QListWidgetItem;
    blank->setText("(no visible exits)");
    blank->setFlags(Qt::NoItemFlags);
    exits->addItem(blank);
  }
}

void RoomView::exploreRoom(QListWidgetItem* item)
{
  emit exploreRoom(item->data(Qt::UserRole).toInt(), item->data(Qt::UserRole + 1).toString());
}

int RoomView::exitDestination(const QString& dir) const
{
  const MapRoom* room = mapManager ? mapManager->room(currentRoomId) : nullptr;
  if (!room) {
    return -1;
  }
  QString norm = MapRoom::normalizeDir(dir);
  if (room->exits.contains(norm)) {
    MapExit exit = room->exits.value(norm);
    return exit.dest;
  }
  return -1;
}

void RoomView::setResponseMessage(const QString& message, bool isError)
{
  if (message.isEmpty()) {
    responseMessage->setVisible(false);
    static_cast<QGridLayout*>(layout())->setRowStretch(1, 0);
  } else {
    QPalette p = palette();
    if (isError) {
      p.setColor(QPalette::Text, QColor(192, 0, 0));
    }
    responseMessage->setPalette(p);
    responseMessage->setVisible(true);
    responseMessage->setText(message);
    static_cast<QGridLayout*>(layout())->setRowStretch(1, 1);
    QTimer::singleShot(0, [this]{ responseMessage->verticalScrollBar()->setValue(0); });
  }
}
