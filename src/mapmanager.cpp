#include "mapmanager.h"
#include <QSettings>

static const QMap<QString, QString> dirAbbrev{
  { "NORTH", "N" },
  { "WEST", "W" },
  { "SOUTH", "S" },
  { "EAST", "E" },
  { "UP", "U" },
  { "DOWN", "D" },
};

MapManager::MapManager(QObject* parent)
: QObject(parent), logRoomDescription(false), logExits(false), roomDirty(false)
{
  // initializers only
}

void MapManager::loadProfile(const QString& profile)
{
  Q_UNUSED(profile);
}

void MapManager::processLine(const QString& line)
{
  if (logRoomDescription) {
    if (line.startsWith("Obvious exits:")) {
      logRoomDescription = false;
      pendingDescription = pendingDescription.trimmed().mid(1);
      if (roomDirty) {
        rooms[currentRoom].description = pendingDescription;
        emit currentRoomUpdated(this, currentRoom);
      }
    } else {
      roomDirty = true;
      pendingDescription += line + "\n";
    }
  } else if (logExits) {
    MapRoom* room = &rooms[currentRoom];
    int pos = line.indexOf(" - ");
    if (pos < 0) {
      logExits = false;
      if (roomDirty) {
        emit currentRoomUpdated(this, currentRoom);
      }
      return;
    }
    QString dest = line.mid(pos + 3).trimmed();
    if (dest.toLower().contains("too dark to tell")) {
      return;
    }
    QString dir = line.left(pos).trimmed().toUpper();
    dir = dirAbbrev.value(dir, dir);
    if (!room->exits.contains(dir)) {
      qDebug() << "XXX: unexpected exit" << dir << dest;
    }
    int destId = room->exits[dir].dest;
    MapRoom* destRoom = &rooms[destId];
    if (destRoom->name.isEmpty()) {
      destRoom->name = dest;
      roomDirty = true;
    }
  } else if (rooms.contains(currentRoom) && rooms[currentRoom].name == line && rooms[currentRoom].description.isEmpty()) {
    logRoomDescription = true;
    pendingDescription = ">";
  } else if (line.trimmed() == "Obvious exits:") {
    logExits = true;
  }
}

void MapManager::msspEvent(const QString& key, const QString& value)
{
  Q_UNUSED(key);
  Q_UNUSED(value);
  qDebug() << "MSSP" << key << value;
}

void MapManager::gmcpEvent(const QString& key, const QVariant& value)
{
  Q_UNUSED(key);
  Q_UNUSED(value);
  if (key.toUpper() == "ROOM") {
    // QVariant(QVariantMap, QMap(("Exits", QVariant(QVariantMap, QMap(("D", QVariant(QVariantMap, QMap(("is_door", QVariant(bool, false))("to_room", QVariant(qlonglong, 3052))))))))("id", QVariant(qlonglong, 3054))("name", QVariant(QString, "The Forest Inn Reception Area"))("type", QVariant(QString, "Structure"))("zone", QVariant(QString, "Mielikki"))))
    updateRoom(value.toMap());
  }
}

void MapManager::updateRoom(const QVariantMap& info)
{
  int roomId = info["id"].toInt();
  MapRoom& room = rooms[roomId];
  room.name = info["name"].toString();
  QVariantMap exits = info["Exits"].toMap();
  // qDebug() << exits;
  for (const QString& dir : exits.keys()) {
    MapExit& exit = room.exits[dir];
    QVariantMap exitInfo = exits[dir].toMap();
    QString status = exitInfo["door"].toString().toUpper();
    exit.dest = exitInfo["to_room"].toInt();
    exit.door = exitInfo["is_door"].toBool();
    exit.locked = exit.door && status == "LOCKED";
    exit.open = exit.door && !exit.locked && status != "CLOSED";
    exit.name = exitInfo["door_name"].toString();
    if (exit.name.isEmpty()) {
      exit.name = "door";
    }
  }
  currentRoom = roomId;
  emit currentRoomUpdated(this, roomId);
}

const MapRoom* MapManager::room(int id)
{
  if (rooms.contains(id)) {
    return &rooms[id];
  }
  return nullptr;
}
