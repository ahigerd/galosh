#include "automapper.h"
#include "mapmanager.h"
#include <QtDebug>

AutoMapper::AutoMapper(MapManager* map, QObject* parent)
: QObject(parent), map(map), logRoomLegacy(false), logRoomDescription(false), logExits(false), roomDirty(false),
  currentRoomId(-1), destinationRoomId(-1), previousRoomId(-1)
{
  QObject::connect(map, SIGNAL(reset()), this, SLOT(reset()));
}

const MapRoom* AutoMapper::currentRoom() const
{
  return map->room(currentRoomId);
}

void AutoMapper::reset()
{
  currentRoomId = -1;
  endRoomCapture();
}

void AutoMapper::commandEntered(const QString& rawCommand, bool echo)
{
  if (map->gmcpMode || !echo) {
    // Using GMCP automapping or entering a password
    return;
  }
  pendingLines.clear();
  if (logRoomLegacy) {
    // Moving too quickly
    qDebug() << "moving too quickly" << rawCommand;
    logRoomLegacy = false;
    return;
  }
  QStringList command = rawCommand.simplified().toUpper().split(' ');
  if (command.length() > 1 && command.first() == "GOTO") {
    qDebug() << "GOTO";
    // TODO: reidentify room heuristics?
    destinationRoomId = map->autoRoomId++;
    logRoomLegacy = true;
    logRoomDescription = false;
    logExits = false;
    return;
  }
  if (command.length() > 1) {
    if (command[0] != "GO" || command.length() > 2) {
      return;
    }
    command.takeFirst();
  }
  QString dir = MapRoom::normalizeDir(command.first());
  bool isLook = QStringLiteral("LOOK").startsWith(dir);
  if (isLook && currentRoomId < 0) {
    // Can't correlate unknown position
    return;
  }
  // TODO: "enter" could have a second parameter
  // TODO: special exits
  if (MapRoom::isDir(dir) || isLook) {
    MapRoom* room = map->mutableRoom(currentRoomId);
    if (room && (dir == "ENTER" || dir == "LEAVE") && room->exits.contains("SOMEWHERE")) {
      dir = "SOMEWHERE";
    }
    destinationDir = dir;
    if (isLook) {
      destinationRoomId = currentRoomId;
    } else if (!room) {
      destinationRoomId = map->autoRoomId++;
    } else if (room->exits.contains(dir)) {
      int dest = room->exits.value(dir).dest;
      if (dest < 0) {
        room->exits[dir].dest = dest = map->autoRoomId++;
      }
      destinationRoomId = dest;
    } else {
      qDebug() << "unknown exit from " << currentRoomId << dir;
      return;
    }
    logRoomLegacy = true;
    logRoomDescription = false;
    logExits = false;
  }
}

void AutoMapper::gmcpEvent(const QString& key, const QVariant& value)
{
  Q_UNUSED(key);
  Q_UNUSED(value);
  if (key.toUpper() == "ROOM") {
    map->gmcpMode = true;
    int roomId = value.toMap().value("id", -1).toInt();
    if (roomId > 0) {
      map->updateRoom(value.toMap());
      currentRoomId = roomId;
      emit currentRoomUpdated(roomId);
    }
  } else if (key.toUpper() == "CLIENT.MAP") {
    QVariantMap info = value.toMap();
    QString url = info.value("url").toString();
    if (!url.isEmpty()) {
      map->downloadMap(url);
    }
  }
}

void AutoMapper::processLine(const QString& line)
{
  if (logRoomLegacy) {
    if (line.startsWith("Obvious exits:") || line.startsWith("Exits:")) {
      if (line.startsWith("Obvious exits:") && pendingLines.isEmpty()) {
        logExits = true;
      } else if (pendingLines.isEmpty()) {
        logRoomLegacy = false;
        return;
      } else {
        // TODO: figure out if we're not where we think we are (description hashing?)
        if (currentRoomId >= 0 && destinationRoomId >= 0 && currentRoomId != destinationRoomId) {
          MapRoom* room = map->mutableRoom(destinationRoomId);
          QString back = MapRoom::reverseDir(destinationDir);
          if (!back.isEmpty() && room->exits.contains(back)) {
            if (room->exits[back].dest < 0) {
              room->exits[back].dest = currentRoomId;
            } else if (room->exits[back].dest != currentRoomId) {
              qDebug() << "not where we thought we were!";
            }
          }
        }
        previousRoomId = currentRoomId;
        if (destinationRoomId < 0) {
          currentRoomId = map->autoRoomId++;
        } else {
          currentRoomId = destinationRoomId;
        }
        MapRoom* room = map->mutableRoom(currentRoomId);
        QString roomName = pendingLines.takeFirst();
        if (room->name.isEmpty() || destinationDir == "LOOK") {
          roomDirty = true;
          room->name = roomName;
        } else if (pendingLines.isEmpty() && room->name != roomName) {
          qDebug() << "not a room maybe?";
          endRoomCapture();
          return;
        }
        if (pendingLines.isEmpty()) {
          pendingDescription.clear();
        } else {
          pendingDescription = ">" + pendingLines.join('\n');
          roomDirty = true;
        }
        destinationRoomId = -1;
        logRoomLegacy = false;
        logRoomDescription = true;
      }
    } else {
      if (pendingLines.isEmpty()) {
        QString parsed = line.simplified();
        while (parsed.length() > 0 && (parsed.back() == ' ' || parsed.back() == '.')) {
          parsed.chop(1);
        }
        if (parsed.isEmpty() || parsed.toLower() == "ok") {
          // ignore blank lines after prompt
          return;
        }
      }
      pendingLines << line;
      return;
    }
  }
  if (logRoomDescription) {
    if (line.startsWith("Obvious exits:") || line.startsWith("Exits:")) {
      logRoomDescription = false;
      pendingDescription = pendingDescription.trimmed().mid(1);
      bool exitsDirty = false;
      if (line.startsWith("Exits:")) {
        QStringList exits = line.mid(6).trimmed().split(' ');
        MapRoom& room = map->rooms[currentRoomId];
        QString back = MapRoom::reverseDir(destinationDir);
        for (QString exit : exits) {
          bool locked = exit.startsWith('[');
          if (locked) {
            exit = exit.mid(1, exit.length() - 2);
          }
          exit = MapRoom::normalizeDir(exit);
          if (!room.exits.contains(exit)) {
            if (exit == back) {
              room.exits[exit].dest = previousRoomId;
            } else {
              room.exits[exit].dest = -1;
            }
            exitsDirty = true;
          }
          if (locked && !room.exits[exit].door) {
            room.exits[exit].door = true;
            room.exits[exit].open = false;
            room.exits[exit].locked = true;
            room.exits[exit].lockable = true;
            exitsDirty = true;
          }
        }
      }
      if (roomDirty || exitsDirty) {
        if (roomDirty) {
          map->rooms[currentRoomId].description = pendingDescription;
        }
        if (!map->gmcpMode) {
          MapZone* zone = map->mutableZone("");
          if (!zone->roomIds.contains(currentRoomId)) {
            zone->roomIds << currentRoomId;
            if (map->mapSearch) {
              map->mapSearch->markDirty(zone);
            }
          }
        }
        map->saveRoom(&map->rooms[currentRoomId]);
      }
      // TODO: emit currentRoomIdUpdated(this, currentRoomId);
    } else {
      roomDirty = true;
      pendingDescription += line + "\n";
    }
  } else if (logExits) {
    MapRoom* room = &map->rooms[currentRoomId];
    int pos = line.indexOf(" - ");
    if (pos < 0) {
      logExits = false;
      endRoomCapture();
      if (roomDirty) {
        map->saveRoom(&map->rooms[currentRoomId]);
        // TODO: emit currentRoomIdUpdated(this, currentRoomId);
      }
      return;
    }
    QString dest = line.mid(pos + 3).trimmed();
    if (dest.toLower().contains("too dark to tell")) {
      return;
    }
    QString dir = MapRoom::normalizeDir(line.left(pos));
    if (!room->exits.contains(dir)) {
      qDebug() << "XXX: unexpected exit" << dir << dest;
    }
    int destId = room->exits[dir].dest;
    if (destId < 0) {
      room->exits[dir].dest = destId = map->autoRoomId++;
    }
    MapRoom* destRoom = &map->rooms[destId];
    if (destRoom->name.isEmpty()) {
      // TODO: in legacy mode, detect if we're in an unexpected place
      destRoom->name = dest;
      roomDirty = true;
    }
  } else if (map->rooms.contains(currentRoomId) && map->rooms[currentRoomId].name == line && map->rooms[currentRoomId].description.isEmpty()) {
    logRoomDescription = true;
    pendingDescription = ">";
  } else if (line.trimmed() == "Obvious exits:") {
    logExits = true;
  }
}

void AutoMapper::endRoomCapture()
{
  if (currentRoomId >= 0) {
    emit currentRoomUpdated(currentRoomId);
  }
  destinationRoomId = -1;
  previousRoomId = -1;
  pendingDescription.clear();
  pendingLines.clear();
  roomDirty = false;
  destinationDir.clear();
  logRoomLegacy = false;
  logRoomDescription = false;
  logExits = false;
}

void AutoMapper::promptWaiting()
{
  if (logRoomLegacy) {
    // We didn't get a recognized room description, and now we have a prompt
    pendingLines.clear();
    logRoomLegacy = false;
  }
}
