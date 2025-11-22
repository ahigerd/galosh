#include "mapmanager.h"
#include "mudletimport.h"
#include "mapviewer.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QDir>
#include <QStandardPaths>
#include <time.h>
#include <QtDebug>

struct SettingsGroup
{
  SettingsGroup(QSettings* settings, const QString& group)
  : settings(settings)
  {
    settings->beginGroup(group);
  }

  ~SettingsGroup()
  {
    settings->endGroup();
  }

  QSettings* settings;
};

QString MapManager::mapForProfile(const QString& profile)
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
  return mapFileName;
}

MapManager::MapManager(QObject* parent)
: QObject(parent), mapFile(nullptr), gmcpMode(false), logRoomLegacy(false), logRoomDescription(false), logExits(false), roomDirty(false),
  autoRoomId(1), currentRoom(-1), destinationRoom(-1), previousRoom(-1), mapSearch(nullptr)
{
  // initializers only
}

void MapManager::loadProfile(const QString& profile)
{
  loadMap(mapForProfile(profile));
}

void MapManager::loadMap(const QString& mapFileName)
{
  if (mapFile) {
    mapFile->deleteLater();
  }
  mapFile = new QSettings(mapFileName, QSettings::IniFormat, this);
  mapSearch.reset();
  rooms.clear();
  endRoomCapture();
  autoRoomId = mapFile->value("autoID", 1).toInt();

  for (const QString& zone : mapFile->childGroups()) {
    if (zone.startsWith(" ")) {
      continue;
    }
    QString zoneName = zone == "-" ? "" : zone;
    SettingsGroup zoneGroup(mapFile, zone);
    MapZone* zoneObj = mutableZone(zoneName);
    for (const QString& idStr : mapFile->childGroups()) {
      int id = idStr.toInt();
      if (!id) {
        qDebug() << "Unexpected room ID" << idStr << "in zone" << zone;
        continue;
      }
      SettingsGroup idGroup(mapFile, idStr);
      MapRoom* room = mutableRoom(id);
      room->zone = zoneName;
      room->name = mapFile->value("name").toString();
      room->description = mapFile->value("description").toString();
      SettingsGroup exitGroup(mapFile, "exit");
      for (const QString& dir : mapFile->childGroups()) {
        SettingsGroup dirGroup(mapFile, dir);
        MapExit exit;
        exit.name = mapFile->value("name").toString();
        exit.dest = mapFile->value("id").toInt();
        exit.door = mapFile->value("door").toBool();
        exit.lockable = mapFile->value("lock").toBool();
        exit.open = false;
        exit.locked = exit.lockable;
        room->exits[dir] = exit;
      }
      zoneObj->addRoom(room);
    }
  }

  for (const MapRoom& room : rooms) {
    for (const MapExit& exit : room.exits) {
      if (rooms.contains(exit.dest)) {
        rooms[exit.dest].entrances << room.id;
      }
    }
  }
}

void MapManager::promptWaiting()
{
  if (logRoomLegacy) {
    // We didn't get a recognized room description, and now we have a prompt
    pendingLines.clear();
    logRoomLegacy = false;
  }
}

void MapManager::commandEntered(const QString& rawCommand, bool echo)
{
  if (gmcpMode || !echo) {
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
    destinationRoom = autoRoomId++;
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
  if (isLook && currentRoom < 0) {
    // Can't correlate unknown position
    return;
  }
  // TODO: "enter" could have a second parameter
  // TODO: special exits
  if (MapRoom::isDir(dir) || isLook) {
    if (currentRoom >= 0 && (dir == "ENTER" || dir == "LEAVE") && rooms[currentRoom].exits.contains("SOMEWHERE")) {
      dir = "SOMEWHERE";
    }
    destinationDir = dir;
    if (isLook) {
      destinationRoom = currentRoom;
    } else if (currentRoom < 0) {
      destinationRoom = autoRoomId++;
    } else if (rooms[currentRoom].exits.contains(dir)) {
      MapRoom* room = mutableRoom(currentRoom);
      int dest = room->exits.value(dir).dest;
      if (dest < 0) {
        room->exits[dir].dest = dest = autoRoomId++;
      }
      destinationRoom = dest;
    } else {
      qDebug() << "unknown exit from " << currentRoom << dir;
      return;
    }
    logRoomLegacy = true;
    logRoomDescription = false;
    logExits = false;
  }
}

void MapManager::processLine(const QString& line)
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
        if (currentRoom >= 0 && destinationRoom >= 0 && currentRoom != destinationRoom) {
          MapRoom* room = mutableRoom(destinationRoom);
          QString back = MapRoom::reverseDir(destinationDir);
          if (!back.isEmpty() && room->exits.contains(back)) {
            if (room->exits[back].dest < 0) {
              room->exits[back].dest = currentRoom;
            } else if (room->exits[back].dest != currentRoom) {
              qDebug() << "not where we thought we were!";
            }
          }
        }
        previousRoom = currentRoom;
        if (destinationRoom < 0) {
          currentRoom = autoRoomId++;
        } else {
          currentRoom = destinationRoom;
        }
        MapRoom* room = mutableRoom(currentRoom);
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
        destinationRoom = -1;
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
        MapRoom& room = rooms[currentRoom];
        QString back = MapRoom::reverseDir(destinationDir);
        for (QString exit : exits) {
          bool locked = exit.startsWith('[');
          if (locked) {
            exit = exit.mid(1, exit.length() - 2);
          }
          exit = MapRoom::normalizeDir(exit);
          if (!room.exits.contains(exit)) {
            if (exit == back) {
              room.exits[exit].dest = previousRoom;
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
          rooms[currentRoom].description = pendingDescription;
        }
        saveRoom(&rooms[currentRoom]);
      }
      emit currentRoomUpdated(this, currentRoom);
    } else {
      roomDirty = true;
      pendingDescription += line + "\n";
    }
  } else if (logExits) {
    MapRoom* room = &rooms[currentRoom];
    int pos = line.indexOf(" - ");
    if (pos < 0) {
      logExits = false;
      endRoomCapture();
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
    QString dir = MapRoom::normalizeDir(line.left(pos));
    if (!room->exits.contains(dir)) {
      qDebug() << "XXX: unexpected exit" << dir << dest;
    }
    int destId = room->exits[dir].dest;
    if (destId < 0) {
      room->exits[dir].dest = destId = autoRoomId++;
    }
    MapRoom* destRoom = &rooms[destId];
    if (destRoom->name.isEmpty()) {
      // TODO: in legacy mode, detect if we're in an unexpected place
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

void MapManager::gmcpEvent(const QString& key, const QVariant& value)
{
  Q_UNUSED(key);
  Q_UNUSED(value);
  if (key.toUpper() == "ROOM") {
    gmcpMode = true;
    updateRoom(value.toMap());
  } else if (key.toUpper() == "CLIENT.MAP") {
    QVariantMap map = value.toMap();
    QString url = map.value("url").toString();
    if (!url.isEmpty()) {
      downloadMap(url);
    }
  }
}

void MapManager::updateRoom(const QVariantMap& info)
{
  QString zoneName = info["zone"].toString();

  int roomId = info["id"].toInt();
  bool newRoom = !rooms.contains(roomId);
  MapRoom& room = rooms[roomId];
  room.id = roomId;
  room.name = info["name"].toString();
  room.zone = zoneName;
  room.roomType = info["type"].toString();

  QVariantMap exits = info["Exits"].toMap();
  for (const QString& dir : exits.keys()) {
    newRoom = newRoom || !room.exits.contains(dir);
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

  MapZone* zone = mutableZone(zoneName);
  zone->addRoom(&room);

  if (mapFile) {
    saveRoom(&room);
  }
  if (newRoom && mapSearch) {
    mapSearch->markDirty(zone);
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

const MapRoom* MapManager::room()
{
  return room(currentRoom);
}

MapRoom* MapManager::mutableRoom(int id)
{
  MapRoom* room = &rooms[id];
  room->id = id;
  return room;
}

QList<const MapRoom*> MapManager::searchForRooms(const QStringList& args, bool namesOnly, const QString& zone) const
{
  if (args.isEmpty()) {
    return {};
  }
  QList<const MapRoom*> filtered;
  QRegularExpression re(args.first(), QRegularExpression::CaseInsensitiveOption);
  QString matchZone;
  if (!zone.isEmpty()) {
    const MapZone* zoneObj = searchForZone(zone);
    if (!zoneObj) {
      return {};
    }
    matchZone = zoneObj->name;
  }
  for (const MapRoom& room : rooms) {
    if (!matchZone.isEmpty() && room.zone != matchZone) {
      continue;
    }
    if (re.match(room.name).hasMatch()) {
      filtered << &room;
    } else if (!namesOnly && re.match(room.description).hasMatch()) {
      filtered << &room;
    }
  }
  for (const QString& arg : args.mid(1)) {
    re.setPattern(arg);
    QList<const MapRoom*> nextRound;
    for (const MapRoom* room : filtered) {
      if (re.match(room->name).hasMatch()) {
        nextRound << room;
      } else if (!namesOnly && re.match(room->description).hasMatch()) {
        nextRound << room;
      }
    }
    filtered = nextRound;
  }
  return filtered;
}

QStringList MapManager::zoneNames() const
{
  QStringList names;
  for (const auto& iter : zones) {
    names << iter.second.name;
  }
  return names;
}

const MapZone* MapManager::zone(const QString& name) const
{
  auto iter = zones.find(name.simplified());
  if (iter != zones.end()) {
    return &iter->second;
  }
  return nullptr;
}

MapZone* MapManager::mutableZone(const QString& name)
{
  QString key = name.simplified();
  auto iter = zones.find(key);
  if (iter != zones.end()) {
    return &iter->second;
  }
  auto [newIter, ok] = zones.try_emplace(key, this, key);
  return &newIter->second;
}

const MapZone* MapManager::searchForZone(const QString& name) const
{
  const MapZone* zone = nullptr;
  QString bestMatch;
  QString match = name.simplified().replace(" ", "").toUpper();
  for (const auto& iter : zones) {
    QString check = QString(iter.first).replace(" ", "").toUpper();
    if (check.startsWith(match)) {
      if (bestMatch.isEmpty() || check.length() < bestMatch.length()) {
        bestMatch = iter.first;
        zone = &iter.second;
      }
    }
  }
  return zone;
}

void MapManager::endRoomCapture()
{
  destinationRoom = -1;
  previousRoom = -1;
  pendingDescription.clear();
  pendingLines.clear();
  roomDirty = false;
  destinationDir.clear();
  logRoomLegacy = false;
  logRoomDescription = false;
  logExits = false;
}

void MapManager::saveRoom(MapRoom* room)
{
  if (!mapFile) {
    qWarning("Cannot save room when no map file is loaded");
    return;
  }

  if (!gmcpMode) {
    mapFile->setValue("autoID", autoRoomId);
  }

  if (room->zone.isEmpty() && gmcpMode) {
    qWarning() << "No zone in GMCP mode";
  }

  SettingsGroup zoneGroup(mapFile, room->zone.isEmpty() ? "-" : room->zone);
  mapFile->setValue("name", room->name);
  mapFile->setValue("description", room->description);
  mapFile->setValue("type", room->roomType);

  SettingsGroup exitGroup(mapFile, "exit");

  for (const QString& dir : room->exits.keys()) {
    SettingsGroup dirGroup(mapFile, dir);
    MapExit& exit = room->exits[dir];
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
  }
}


MapSearch* MapManager::search()
{
  MapSearch* s = mapSearch.get();
  if (!s) {
    s = new MapSearch(this);
    s->precompute();
    mapSearch.reset(s);
  }
  return s;
}

int MapManager::waypoint(const QString& name, QString* canonicalName) const
{
  if (!mapFile) {
    return -1;
  }
  QString search = name.simplified().toLower();
  SettingsGroup g(mapFile, " Waypoints");
  for (const QString& key : mapFile->childKeys()) {
    if (key.simplified().toLower().startsWith(search)) {
      if (canonicalName) {
        *canonicalName = key;
      }
      return mapFile->value(key).toInt();
    }
  }
  return -1;
}

QStringList MapManager::waypoints() const
{
  if (!mapFile) {
    return {};
  }
  SettingsGroup g(mapFile, " Waypoints");
  QStringList names = mapFile->childKeys();
  return names;
}

bool MapManager::setWaypoint(const QString& name, int roomId)
{
  if (!mapFile) {
    return false;
  }
  if (!room(roomId)) {
    return false;
  }
  SettingsGroup g(mapFile, " Waypoints");
  mapFile->setValue(name, roomId);
  return true;
}

bool MapManager::removeWaypoint(const QString& name)
{
  if (!mapFile) {
    return false;
  }
  SettingsGroup g(mapFile, " Waypoints");
  if (!mapFile->contains(name)) {
    return false;
  }
  mapFile->remove(name);
  return true;
}

class MapDownloader : public QObject
{
Q_OBJECT
public:
  MapDownloader(MapManager* map, QSettings* mapFile, const QString& url)
  : QObject(map), map(map), mapFile(mapFile), url(url), qnam(new QNetworkAccessManager(map)), aborted(false)
  {
    urlKey = "downloaded-" + QString(url).replace(QRegularExpression("[/\\:]+"), "_");

    if (mapFile->contains(urlKey)) {
      lastDownloaded = QDateTime::fromString(mapFile->value(urlKey).toString(), Qt::ISODate);
      if (lastDownloaded > QDateTime::currentDateTimeUtc().addDays(-1)) {
        // TODO: config option to force download
        qDebug() << "Skipping recently downloaded map" << url;
        return;
      }
    }

    qDebug() << "Fetching map from" << url;
    qnam->setRedirectPolicy(QNetworkRequest::UserVerifiedRedirectPolicy);
    reply = qnam->get(QNetworkRequest(QUrl(url)));
    QObject::connect(reply, SIGNAL(redirected(QUrl)), reply, SIGNAL(redirectAllowed()));
    headersConn = QObject::connect(reply, &QIODevice::readyRead, [this]{ headersReceived(); });
    QObject::connect(reply, &QNetworkReply::finished, [this]{ onFinished(); });
  }

private slots:
  void headersReceived()
  {
    QObject::disconnect(headersConn);
    QDateTime lastUpdated = reply->header(QNetworkRequest::LastModifiedHeader).toDateTime();
    if (!lastDownloaded.isNull() && lastDownloaded > lastUpdated) {
      qDebug() << "Map data unchanged since last download" << url;
      mapFile->setValue(urlKey, QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
      aborted = true;
      reply->abort();
    }
  }

  void onFinished()
  {
    if (!aborted) {
      MudletImport imp(map, reply);
      if (!imp.importError().isEmpty()) {
        qDebug() << imp.importError();
      }
    }
    mapFile->setValue(urlKey, QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    reply->deleteLater();
    qnam->deleteLater();
    deleteLater();
  }

private:
  MapManager* map;
  QSettings* mapFile;
  QString url;
  QString urlKey;
  QDateTime lastDownloaded;

  QNetworkAccessManager* qnam;
  QNetworkReply* reply;
  bool aborted;
  QMetaObject::Connection headersConn;
};

void MapManager::downloadMap(const QString& url)
{
  if (mapFile) {
    new MapDownloader(this, mapFile, url);
  }
}

#include "mapmanager.moc"
