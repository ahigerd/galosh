#include "mapmanager.h"
#include "mudletimport.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QDir>
#include <QStandardPaths>
#include <QtDebug>

static const QMap<QString, QString> dirAbbrev{
  { "NORTH", "N" },
  { "WEST", "W" },
  { "SOUTH", "S" },
  { "EAST", "E" },
  { "UP", "U" },
  { "DOWN", "D" },
};

MapManager::MapManager(QObject* parent)
: QObject(parent), mapFile(nullptr), currentRoom(-1), logRoomDescription(false), logExits(false), roomDirty(false)
{
  // initializers only
}

void MapManager::loadProfile(const QString& profile)
{
  QSettings settings(profile, QSettings::IniFormat);
  settings.beginGroup("Profile");
  QString mapFileName = settings.value("host").toString();
  if (mapFileName.isEmpty()) {
    mapFileName = profile + "_map";
  } else {
    QDir dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    mapFileName = dir.absoluteFilePath(mapFileName + ".galosh_map");
  }
  loadMap(mapFileName);
}

void MapManager::loadMap(const QString& mapFileName)
{
  if (mapFile) {
    mapFile->deleteLater();
  }
  mapFile = new QSettings(mapFileName, QSettings::IniFormat, this);
  rooms.clear();
  pendingDescription.clear();
  currentRoom = -1;
  logRoomDescription = false;
  logExits = false;
  roomDirty = false;

  for (const QString& zone : mapFile->childGroups()) {
    mapFile->beginGroup(zone);
    for (const QString& idStr : mapFile->childGroups()) {
      int id = idStr.toInt();
      if (!id) {
        qDebug() << "Unexpected room ID" << idStr << "in zone" << zone;
        continue;
      }
      mapFile->beginGroup(idStr);
      MapRoom& room = *mutableRoom(id);
      room.zone = zone;
      room.name = mapFile->value("name").toString();
      room.description = mapFile->value("description").toString();
      mapFile->beginGroup("exit");
      for (const QString& dir : mapFile->childGroups()) {
        mapFile->beginGroup(dir);
        MapExit exit;
        exit.name = mapFile->value("name").toString();
        exit.dest = mapFile->value("id").toInt();
        exit.door = mapFile->value("door").toBool();
        exit.lockable = mapFile->value("lock").toBool();
        exit.open = false;
        exit.locked = exit.lockable;
        room.exits[dir] = exit;
        mapFile->endGroup();
      }
      mapFile->endGroup();
      mapFile->endGroup();
    }
    mapFile->endGroup();
  }
}

void MapManager::processLine(const QString& line)
{
  if (logRoomDescription) {
    if (line.startsWith("Obvious exits:")) {
      logRoomDescription = false;
      pendingDescription = pendingDescription.trimmed().mid(1);
      if (roomDirty) {
        rooms[currentRoom].description = pendingDescription;
        saveRoom(&rooms[currentRoom]);
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
        saveRoom(&rooms[currentRoom]);
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
  } else if (key.toUpper() == "CLIENT.MAP") {
    // QVariant(QVariantMap, QMap(("url", QVariant(QString, "http://www.fierymud.org/mudlet/default_map.dat"))))
    QVariantMap map = value.toMap();
    QString url = map.value("url").toString();
    if (!url.isEmpty()) {
      downloadMap(url);
    }
  }
}

void MapManager::downloadMap(const QString& url)
{
  if (!mapFile) {
    return;
  }
  QStringList downloadedUrls = mapFile->value("*/downloaded").toStringList();
  if (downloadedUrls.contains(url)) {
    // TODO: config option to always download / force download
    qDebug() << "Skipping download of known map" << url;
    return;
  }
  QNetworkAccessManager* qnam = new QNetworkAccessManager(this);
  QObject::connect(qnam, &QNetworkAccessManager::finished, [=](QNetworkReply* reply){
    MudletImport imp(this, reply);
    if (!imp.importError().isEmpty()) {
      qDebug() << imp.importError();
    }
    reply->deleteLater();
    qnam->deleteLater();
    QStringList downloadedUrls = mapFile->value("*/downloaded").toStringList();
    if (!downloadedUrls.contains(url)) {
      downloadedUrls << url;
    }
    mapFile->setValue("*/downloaded", downloadedUrls);
  });
  qDebug() << "Fetching map from" << url;
  qnam->get(QNetworkRequest(QUrl(url)));
}

void MapManager::updateRoom(const QVariantMap& info)
{
  int roomId = info["id"].toInt();
  MapRoom& room = rooms[roomId];
  room.id = roomId;
  room.name = info["name"].toString();
  room.zone = info["zone"].toString();
  room.roomType = info["type"].toString();

  QVariantMap exits = info["Exits"].toMap();
  for (const QString& dir : exits.keys()) {
    MapExit& exit = room.exits[dir];
    QVariantMap exitInfo = exits[dir].toMap();
    QString status = exitInfo["door"].toString().toUpper();
    exit.dest = exitInfo["to_room"].toInt();
    exit.door = exitInfo["is_door"].toBool();
    exit.locked = exit.door && status == "LOCKED";
    exit.lockable = exit.lockable || exit.locked;
    exit.open = exit.door && !exit.locked && status != "CLOSED";
    exit.name = exitInfo["door_name"].toString();
    if (exit.name.isEmpty()) {
      exit.name = "door";
    }
  }

  if (mapFile) {
    saveRoom(&room);
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

MapRoom* MapManager::mutableRoom(int id)
{
  MapRoom* room = &rooms[id];
  room->id = id;
  return room;
}

void MapManager::saveRoom(MapRoom* room)
{
  if (!mapFile) {
    qWarning("Cannot save room when no map file is loaded");
    return;
  }
  mapFile->beginGroup(room->zone);
  mapFile->beginGroup(QString::number(room->id));
  mapFile->setValue("name", room->name);
  mapFile->setValue("description", room->description);
  mapFile->beginGroup("exit");

  for (const QString& dir : room->exits.keys()) {
    MapExit& exit = room->exits[dir];
    mapFile->beginGroup(dir);
    mapFile->setValue("id", exit.dest);
    if (exit.door) {
      if (!exit.name.isEmpty()) {
        mapFile->setValue("name", exit.name);
      }
      mapFile->setValue("door", true);
      if (exit.lockable) {
        mapFile->setValue("lockable", true);
      }
    } else {
      mapFile->remove("door");
      mapFile->remove("name");
      mapFile->remove("lockable");
    }
    mapFile->endGroup();
  }
  mapFile->endGroup();
  mapFile->endGroup();
  mapFile->endGroup();
}
