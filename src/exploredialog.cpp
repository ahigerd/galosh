#include "exploredialog.h"
#include "mapmanager.h"
#include "mapzone.h"
#include "roomview.h"
#include "mapviewer.h"
#include "commandline.h"
#include "commands/slotcommand.h"
#include "commands/mapsearchcommand.h"
#include "commands/maphistorycommand.h"
#include "commands/routecommand.h"
#include "commands/simplifycommand.h"
#include "commands/zonecommand.h"
#include <QSettings>
#include <QGridLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QSplitter>
#include <QMetaObject>
#include <algorithm>
#include <QtDebug>

ExploreDialog::ExploreDialog(MapManager* map, int roomId, int lastRoomId, const QString& movement, QWidget* parent)
: QDialog(parent), map(map), history(map)
{
  setWindowFlag(Qt::Window, true);
  setAttribute(Qt::WA_WindowPropagation, true);
  setAttribute(Qt::WA_DeleteOnClose, true);

  QVBoxLayout* vbox = new QVBoxLayout(this);
  vbox->setContentsMargins(0, 0, 0, 0);

  splitter = new QSplitter(Qt::Vertical, this);
  QObject::connect(splitter, SIGNAL(splitterMoved(int,int)), this, SLOT(saveState()));
  vbox->addWidget(splitter, 1);

  mapView = new MapViewer(MapViewer::EmbedMap, map, &history, this);
  splitter->addWidget(mapView);

  QWidget* w = new QWidget(splitter);
  splitter->addWidget(w);

  QGridLayout* layout = new QGridLayout(w);
  QMargins margins = layout->contentsMargins();
  margins.setTop(margins.top() + 2);
  layout->setContentsMargins(margins);

  room = new RoomView(map, roomId, lastRoomId, w);
  room->layout()->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(room, 0, 0, 1, 2);

  line = new CommandLine(w);
  line->setParsing(false); // we'll handle the parsing here
  layout->addWidget(line, 1, 0);

  backButton = new QPushButton("&Back", w);
  backButton->setDefault(false);
  backButton->setAutoDefault(false);
  layout->addWidget(backButton, 1, 1);

  layout->setRowStretch(0, 1);
  layout->setRowStretch(1, 0);
  layout->setColumnStretch(0, 1);
  layout->setColumnStretch(1, 0);

  QObject::connect(room, SIGNAL(roomUpdated(QString, int, QString)), this, SLOT(roomUpdated(QString, int, QString)));
  QObject::connect(room, SIGNAL(exploreRoom(int, QString)), this, SIGNAL(exploreRoom(int, QString)));
  QObject::connect(backButton, SIGNAL(clicked()), this, SLOT(goBack()));
  QObject::connect(line, SIGNAL(returnPressed()), this, SLOT(doCommand()));

  const MapRoom* room = nullptr;
  if (lastRoomId != -1) {
    history.goTo(lastRoomId);
    room = map->room(lastRoomId);
  }
  if (roomId != -1) {
    history.travel(movement, roomId);
    room = map->room(lastRoomId);
  }
  if (room) {
    setWindowTitle(room->name);
  } else {
    setWindowTitle("Unknown location");
  }

  line->setFocus();

  addCommand(new MapSearchCommand(map));
  addCommand(new ZoneCommand(map));
  addCommand(new MapHistoryCommand("HISTORY", &history));
  addCommand(new MapHistoryCommand("REVERSE", &history));
  addCommand(new MapHistoryCommand("SPEED", &history));
  addCommand(new RouteCommand(map, &history));
  addCommand(new SimplifyCommand(&history));
  addCommand(new SlotCommand("BACK", this, SLOT(goBack()), "Returns to the previous room"));
  addCommand(new SlotCommand("GOTO", this, SLOT(goToRoom(QString)), "Teleports to a room by numeric ID"));
  addCommand(new SlotCommand("RESET", &history, SLOT(reset()), "Clears the exploration history"));

  QSettings settings;
  restoreGeometry(settings.value("explore").toByteArray());
  splitter->restoreState(settings.value("exploreSplit").toByteArray());
}

void ExploreDialog::roomUpdated(const QString& title, int id, const QString& movement)
{
  setWindowTitle(title);
  history.travel(movement, id);
  backButton->setEnabled(history.canGoBack());
}

void ExploreDialog::doCommand()
{
  responseLines.clear();
  responseError = false;
  expectResponse = true;
  clearResponse();
  QString command = line->text();
  if (command.startsWith("/")) {
    // politely ignore slashes
    command.remove(0, 1);
  }
  handleCommand(command);
  if (expectResponse) {
    setResponse(responseError, responseLines.join("\n"));
  }
  line->selectAll();
}

void ExploreDialog::handleSpeedwalk(const QStringList& dirs)
{
  if (dirs.isEmpty()) {
    setResponse(true);
    return;
  }
  QStringList messages;
  bool error = false;
  QString lastRoom;
  for (int i = 0; i < dirs.length(); i++) {
    QString dir = MapRoom::normalizeDir(dirs[i]);
    int dest = history.travel(dir);
    if (dest < 0) {
      error = true;
      messages << QStringLiteral("Unable to travel %1 from this location").arg(dir);
      if (i + 1 < dirs.length()) {
        messages << QStringLiteral("Remaining steps: %1").arg(dirs.mid(i + 1).join(" "));
      }
      break;
    }
    lastRoom = RoomView::formatRoomTitle(history.currentRoom());
    messages << QStringLiteral("%1 to %2").arg(dir).arg(lastRoom);
  }
  const MapRoom* room = history.currentRoom();
  roomView()->setRoom(map, room ? room->id : -1);
  setResponse(error, messages.join("\n"));
}

bool ExploreDialog::commandFilter(const QString& command, const QStringList& args)
{
  if (command.startsWith(".")) {
    handleSpeedwalk(CommandLine::parseSpeedwalk(command + args.join("")));
    return true;
  }

  QString dir = MapRoom::normalizeDir(command);
  if (MapRoom::isDir(dir)) {
    int dest = history.travel(dir);
    if (dest < 0) {
      responseError = true;
    } else {
      emit exploreRoom(dest, dir);
    }
    return true;
  }

  return false;
}

void ExploreDialog::showCommandMessage(TextCommand* command, const QString& message, bool isError)
{
  if (command && isError) {
    responseLines << QString("%1: %2").arg(command->name(), message);
  } else {
    responseLines << message;
  }
  responseError = responseError || isError;
}

void ExploreDialog::refocus()
{
  show();
  raise();
  setFocus();
  line->setFocus();
  line->selectAll();
}

void ExploreDialog::setResponse(bool isError, const QString& message)
{
  expectResponse = false;
  QPalette p = palette();
  if (isError) {
    p.setColor(QPalette::Base, QColor(255, 128, 128));
    QTimer::singleShot(100, this, SLOT(setResponse()));
    line->selectAll();
    line->setFocus();
  }
  line->setPalette(p);
  if (!message.isEmpty() || isError) {
    room->setResponseMessage(message, isError);
  }
}

void ExploreDialog::clearResponse()
{
  room->setResponseMessage("", false);
}

void ExploreDialog::goBack()
{
  if (!history.canGoBack()) {
    setResponse(true, "No room to go back to.");
    return;
  }
  int roomId = history.back();
  const MapRoom* room = map->room(roomId);
  if (!room) {
    setResponse(true, "Could not load destination room.");
    return;
  }
  emit exploreRoom(roomId, QString());
  clearResponse();
}

void ExploreDialog::goToRoom(const QString& id)
{
  int roomId = id.toInt();
  if (roomId <= 0) {
    setResponse(true, "Invalid room ID: " + id);
  }
  emit exploreRoom(roomId, QString());
}

void ExploreDialog::saveState()
{
  QSettings settings;
  settings.setValue("explore", saveGeometry());
  settings.setValue("exploreSplit", splitter->saveState());
}

void ExploreDialog::resizeEvent(QResizeEvent*)
{
  saveState();
}

void ExploreDialog::moveEvent(QMoveEvent*)
{
  saveState();
}
