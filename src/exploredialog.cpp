#include "exploredialog.h"
#include "mapmanager.h"
#include "roomview.h"
#include <QGridLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>

ExploreDialog::ExploreDialog(MapManager* map, int roomId, int lastRoomId, QWidget* parent)
: QDialog(parent), map(map)
{
  setWindowFlag(Qt::Dialog, true);
  setWindowFlag(Qt::WindowStaysOnTopHint, true);
  setAttribute(Qt::WA_WindowPropagation, true);
  setAttribute(Qt::WA_WindowPropagation, true);
  setAttribute(Qt::WA_DeleteOnClose, true);

  QGridLayout* layout = new QGridLayout(this);
  QMargins margins = layout->contentsMargins();
  margins.setTop(margins.top() + 2);
  layout->setContentsMargins(margins);

  room = new RoomView(map, roomId, lastRoomId, this);
  room->layout()->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(room, 0, 0, 1, 2);

  line = new QLineEdit(this);
  layout->addWidget(line, 1, 0);

  backButton = new QPushButton("&Back", this);
  backButton->setDefault(false);
  backButton->setAutoDefault(false);
  layout->addWidget(backButton, 1, 1);

  layout->setRowStretch(0, 1);
  layout->setRowStretch(1, 0);
  layout->setColumnStretch(0, 1);
  layout->setColumnStretch(1, 0);

  QObject::connect(room, SIGNAL(roomUpdated(QString, int)), this, SLOT(roomUpdated(QString, int)));
  QObject::connect(room, SIGNAL(exploreRoom(int)), this, SIGNAL(exploreRoom(int)));
  QObject::connect(backButton, SIGNAL(clicked()), this, SLOT(goBack()));
  QObject::connect(line, SIGNAL(returnPressed()), this, SLOT(doCommand()));

  const MapRoom* room;
  if (lastRoomId != -1) {
    roomHistory << lastRoomId;
    room = map->room(lastRoomId);
  }
  if (roomId != -1) {
    roomHistory << roomId;
    room = map->room(lastRoomId);
  }
  if (room) {
    setWindowTitle(room->name);
  } else {
    setWindowTitle("Unknown location");
  }

  line->setFocus();
}

void ExploreDialog::roomUpdated(const QString& title, int id)
{
  setWindowTitle(title);
  roomHistory << id;
  backButton->setEnabled(roomHistory.size() > 1);
}

void ExploreDialog::goBack()
{
  if (roomHistory.size() < 2) {
    setErrorState(true);
    return;
  }
  roomHistory.takeLast();
  int roomId = roomHistory.takeLast();
  emit exploreRoom(roomId);
}

void ExploreDialog::doCommand()
{
  QString dir = line->text().trimmed();
  if (dir.isEmpty()) {
    return;
  }

  if (dir.toUpper() == "BACK") {
    goBack();
    return;
  }

  int dest = room->exitDestination(dir);
  if (dest < 0) {
    setErrorState(true);
  } else {
    emit exploreRoom(dest);
  }
}

void ExploreDialog::refocus()
{
  show();
  raise();
  setFocus();
  line->setFocus();
  line->selectAll();
}

void ExploreDialog::setErrorState(bool on)
{
  QPalette p = palette();
  if (on) {
    p.setColor(QPalette::Base, QColor(255, 128, 128));
    QTimer::singleShot(100, this, SLOT(setErrorState()));
    line->selectAll();
    line->setFocus();
  }
  line->setPalette(p);
}
