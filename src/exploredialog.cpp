#include "exploredialog.h"
#include "mapmanager.h"
#include "mapzone.h"
#include "roomview.h"
#include "commandline.h"
#include <QGridLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QMetaObject>
#include <algorithm>
#include <QtDebug>

ExploreDialog::ExploreDialog(MapManager* map, int roomId, int lastRoomId, const QString& movement, QWidget* parent)
: QDialog(parent), map(map), history(map)
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

  QObject::connect(room, SIGNAL(roomUpdated(QString, int, QString)), this, SLOT(roomUpdated(QString, int, QString)));
  QObject::connect(room, SIGNAL(exploreRoom(int, QString)), this, SIGNAL(exploreRoom(int, QString)));
  QObject::connect(backButton, SIGNAL(clicked()), this, SLOT(do_BACK()));
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
}

void ExploreDialog::roomUpdated(const QString& title, int id, const QString& movement)
{
  setWindowTitle(title);
  history.travel(movement, id);
  backButton->setEnabled(history.canGoBack());
}

void ExploreDialog::doCommand()
{
  QString command = line->text().simplified();
  if (command.startsWith(".")) {
    QStringList dirs = CommandLine::parseSpeedwalk(command);
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
    return;
  }

  QString dir = MapRoom::normalizeDir(command);
  if (dir.isEmpty()) {
    return;
  }

  clearResponse();
  if (MapRoom::isDir(dir)) {
    int dest = history.travel(dir);
    if (dest < 0) {
      setResponse(true);
    } else {
      emit exploreRoom(dest, dir);
    }
  } else {
    QStringList args = dir.split(' ');
    QString command = args.takeFirst();
    handleCommand(command, args);
  }
  line->selectAll();
}

void ExploreDialog::handleCommand(const QString& command, const QStringList& args)
{
  QByteArray slot = QStringLiteral("do_%1").arg(command).toUtf8();
  if (metaObject()->indexOfSlot((slot + "(QStringList)").constData()) >= 0) {
    if (!QMetaObject::invokeMethod(this, slot.constData(), Q_ARG(QStringList, args))) {
      setResponse(true, QStringLiteral("Unknown error running command: %1").arg(command));
    }
  } else if (metaObject()->indexOfSlot((slot + "()").constData()) >= 0) {
    if (args.length()) {
      setResponse(true, QStringLiteral("%1 does not accept any parameters").arg(command));
    } else if (!QMetaObject::invokeMethod(this, slot.constData())) {
      setResponse(true, QStringLiteral("Unknown error running command: %1").arg(command));
    }
  } else {
    setResponse(true, QStringLiteral("Unknown command: %1").arg(command));
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

void ExploreDialog::setResponse(bool isError, const QString& message)
{
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

void ExploreDialog::do_BACK()
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

void ExploreDialog::do_HELP()
{
  QStringList messages;
  messages << "HELP\t\tShow this message";
  messages << "BACK\t\tReturn to the previous room";
  messages << "GOTO\tid\tTeleport to a room by ID";
  messages << "ZONE\t\tList known zones";
  messages << "ZONE\tname\tShow information about a zone";
  messages << "SEARCH\twords\tSearch for rooms with names or descriptions containing words";
  messages << "SEARCH\t-n words\tSearch for rooms with names containing words";
  messages << "RESET\t\tResets the exploration history";
  messages << "SIMPLIFY\t[aggr]\tRemoves backtracking from exploration history";
  messages << "\t\tIf given any parameters, also looks for shortcuts";
  messages << "ROUTE\tid\tFinds a route from the current room to the specified room";
  messages << "HISTORY\t\tDisplays the exploration history. Supported options:";
  messages << "\tnumber\tLimits the number of steps to show (default 10 for non-SPEED)";
  messages << "\tREVERSE\tShow the steps to travel the path in reverse";
  messages << "\tSPEED\tShow the steps in speedwalking syntax";
  messages << "\tALL\tShows the full history (default for SPEED)";
  messages << "\t*\tShortcut for ALL";
  messages << "REVERSE\t\tShortcut for HISTORY REVERSE";
  messages << "SPEED\t\tShortcut for HISTORY SPEED";
  setResponse(false, messages.join("\n"));
}

void ExploreDialog::do_GOTO(const QStringList& args)
{
  // TODO: flesh out
  if (args.length() == 1) {
    int roomId = args[0].toInt();
    emit exploreRoom(roomId, QString());
  }
}

void ExploreDialog::do_ZONE(const QStringList& args)
{
  QStringList messages;
  if (args.isEmpty()) {
    messages = map->zoneNames();
  } else {
    QString zoneName = args.join(' ');
    const MapZone* zone = map->searchForZone(zoneName);
    if (!zone) {
      setResponse(true, "Unknown zone: " + zoneName);
      return;
    }
    messages << zone->name;
    int minRoom = 0, maxRoom = 0, numRooms = zone->roomIds.size();
    for (int roomId : zone->roomIds) {
      if (!minRoom || roomId < minRoom) {
        minRoom = roomId;
      }
      if (!maxRoom || roomId > maxRoom) {
        maxRoom = roomId;
      }
    }
    messages << QStringLiteral("%1 mapped rooms (%2 - %3)").arg(numRooms).arg(minRoom).arg(maxRoom);
    messages << "" << "Connected zones:";
    messages += zone->exits.keys();
  }
  setResponse(false, messages.join("\n"));
}

void ExploreDialog::do_SEARCH(const QStringList&)
{
}

void ExploreDialog::do_HISTORY(const QStringList& args)
{
  int len = 0;
  bool speedwalk = false;
  bool reverse = false;
  QString badArg;
  for (const QString& arg : args) {
    bool isNumeric = false;
    int numeric = arg.toInt(&isNumeric);
    if (isNumeric) {
      if (numeric <= 0 || len != 0) {
        badArg = arg;
        break;
      }
      len = numeric;
    } else if (QStringLiteral("SPEED").startsWith(arg)) {
      speedwalk = true;
    } else if (QStringLiteral("REVERSE").startsWith(arg)) {
      reverse = true;
    } else if (arg == "*" || arg == "ALL") {
      len = -1;
    } else {
      badArg = arg;
      break;
    }
  }
  if (!badArg.isEmpty()) {
    setResponse(true, QStringLiteral("Could not understand \"%1\".").arg(badArg));
    return;
  }

  QStringList messages;
  bool error = false;
  if (speedwalk) {
    QString dirs = history.speedwalk(len, reverse, &messages);
    if (dirs.length() > 1) {
      messages << dirs;
    } else {
      error = true;
    }
  } else {
    messages = history.describe(len, reverse);
  }
  if (messages.isEmpty()) {
    error = true;
    messages << "No history to show";
  }
  setResponse(error, messages.join("\n"));
}

void ExploreDialog::do_REVERSE(const QStringList& args)
{
  do_HISTORY(QStringList(args) << "REVERSE");
}

void ExploreDialog::do_SPEED(const QStringList& args)
{
  do_HISTORY(QStringList(args) << "SPEED");
}

void ExploreDialog::do_RESET()
{
  history.reset();
}

void ExploreDialog::do_SIMPLIFY(const QStringList& args)
{
  bool aggressive = args.length() > 0;
  int before = history.length();
  history.simplify(aggressive);
  int after = history.length();
  if (before == after) {
    if (aggressive) {
      setResponse(true, "No backtracking or shortcuts in exploration history");
    } else {
      setResponse(true, "No backtracking in exploration history");
    }
  } else {
    setResponse(false, QStringLiteral("Simplified history from %1 steps to %2 steps").arg(before).arg(after));
  }
}

void ExploreDialog::do_ROUTE(const QStringList& args)
{
  if (args.size() != 1) {
    setResponse(true, "Destination room ID required");
    return;
  }
  if (!history.currentRoom()) {
    setResponse(true, "Could not find current room");
    return;
  }
  int endRoomId = args.first().toInt();
  const MapRoom* room = map->room(endRoomId);
  if (!room) {
    setResponse(true, "Could not find destination room");
    return;
  }
  if (room == history.currentRoom()) {
    setResponse(true, "Start room and destination room are the same");
    return;
  }
  int startRoomId = history.currentRoom()->id;
  QList<int> route = MapZone::findWorldRoute(map, startRoomId, endRoomId);
  if (route.isEmpty()) {
    setResponse(true, QStringLiteral("Could not find route from %1 to %2").arg(startRoomId).arg(endRoomId));
    return;
  }
  ExploreHistory path(map);
  path.goTo(startRoomId);
  for (int step : route) {
    if (step == startRoomId) {
      continue;
    }
    for (const QString& dir : path.currentRoom()->exits.keys()) {
      if (path.currentRoom()->exits.value(dir).dest == step) {
        path.travel(dir);
        break;
      }
    }
  }
  QStringList warnings;
  QStringList messages;
  messages << path.speedwalk(-1, false, &warnings) << "";
  messages += path.describe(-1);
  setResponse(!warnings.isEmpty(), messages.join("\n"));
}
