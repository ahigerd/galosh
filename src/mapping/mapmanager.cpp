#include "mapmanager.h"
#include "mudletimport.h"
#include "algorithms.h"
#include "settingsgroup.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QDir>
#include <QStandardPaths>
#include <time.h>
#include <QtDebug>

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

QColor MapManager::colorHeuristic(const QString& roomType)
{
  static constexpr QRegularExpression::PatternOptions opt = QRegularExpression::CaseInsensitiveOption | QRegularExpression::DontCaptureOption;
  static QList<QPair<QRegularExpression, QColor>> heuristics = {
    { QRegularExpression("\\bfountains?\\b", opt), QColor(160, 192, 255) },
    { QRegularExpression("\\b(city|town|road|street|avenue|alley)\\b", opt), QColor(224, 224, 224) },
    { QRegularExpression("\\b(shop|store|house|halls?|structure|building|temple|inn|tavern|offices?)\\b", opt), QColor(160, 160, 160) },
    { QRegularExpression("\\b(path|trail|way)\\b", opt), QColor(220, 200, 180) },
    { QRegularExpression("\\b(beach)\\b", opt), QColor(200, 200, 180) },
    { QRegularExpression("\\b(swamp)", opt), QColor(64, 96, 0) },
    { QRegularExpression("\\b(forest|woods?|trees?)\\b", opt), QColor(96, 192, 32) },
    { QRegularExpression("\\b(shallows|ocean|sea|river|lake|lagoon|(under)?water(fall)?|pool|pond|inlet|reefs?)\\b", opt), QColor(128, 192, 255) },
    { QRegularExpression("(hills?|pass)\\b", opt), QColor(192, 160, 128) },
    { QRegularExpression("\\b(mountains?|cliffs?|ravine?)\\b", opt), QColor(180, 140, 80) },
    { QRegularExpression("\\b(cave(rn)?s?\\b|dark.*|under.*)", opt), QColor(140, 100, 180) },
    { QRegularExpression("\\b(plains?\\b|grass|meadows?\\b|fields?\\b)", opt), QColor(128, 255, 0) },
    { QRegularExpression("\\bruins?\\b", opt), QColor(128, 128, 128) },
  };
  for (auto [regexp, color] : heuristics) {
    if (regexp.match(roomType).hasMatch()) {
      return color;
    }
  }
  return QColor();
}

MapManager::MapManager(QObject* parent)
: QObject(parent), mapFile(nullptr), gmcpMode(false), autoRoomId(1), mapSearch(nullptr)
{
  // initializers only
}

void MapManager::loadProfile(const QString& profile)
{
  loadMap(mapForProfile(profile));
}

QSettings* MapManager::mapProfile() const
{
  return mapFile;
}

void MapManager::loadMap(const QString& mapFileName)
{
  if (mapFile) {
    mapFile->deleteLater();
  }
  mapFile = new QSettings(mapFileName, QSettings::IniFormat, this);
  emit reset();
  mapSearch.reset();
  rooms.clear();
  autoRoomId = mapFile->value("autoID", 1).toInt();

  {
    SettingsGroup sg(mapFile, " RoomTypes");
    roomCosts.clear();
    roomColors.clear();
    for (const QString& roomType : mapFile->childGroups()) {
      SettingsGroup rt(mapFile, roomType);
      roomCosts[roomType] = qMax(mapFile->value("cost").toInt(), 1);
      roomColors[roomType] = QColor(mapFile->value("color").toString());
    }
  }

  for (const QString& zone : mapFile->childGroups()) {
    if (zone.startsWith(" ")) {
      continue;
    }
    QString zoneName = zone;
    if (zoneName == "-") {
      zoneName = "";
    } else {
      gmcpMode = true;
    }
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
      room->roomType = mapFile->value("type").toString();
      if (!room->roomType.isEmpty()) {
        if (!roomCosts.contains(room->roomType)) {
          roomCosts[room->roomType] = 1;
        }
        if (!roomColors.contains(room->roomType)) {
          roomColors[room->roomType] = colorHeuristic(room->roomType);
        }
      }
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

  gmcpMode = zones.size() > 1;
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
  if (!room.roomType.isEmpty()) {
    if (!roomCosts.contains(room.roomType)) {
      roomCosts[room.roomType] = 1;
    }
    if (!roomColors.contains(room.roomType)) {
      roomColors[room.roomType] = colorHeuristic(room.roomType);
    }
  }

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

  emit roomUpdated(roomId);
}

const MapRoom* MapManager::room(int id) const
{
  auto iter = rooms.find(id);
  if (iter == rooms.end()) {
    return nullptr;
  }
  return &iter.value();
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
    QString name = iter.second.name;
    if (gmcpMode && (name == "-" || name.isEmpty())) {
      continue;
    }
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
  QStringList match = QString(name).replace(" ","").toUpper().split("-");
  int bestMatchPos = -1;
  for (const auto& iter : zones) {
    QString check = QString(iter.first).replace(" ", "").toUpper();
    int matchPos = 0;
    for (int i = 0; i < match.length(); i++) {
      matchPos = check.indexOf(match[i], matchPos);
      if (matchPos < 0) {
        break;
      }
    }
    if (matchPos < 0) {
      continue;
    }
    if (bestMatch.isEmpty() || bestMatchPos > matchPos || (bestMatchPos == matchPos && check.length() < bestMatch.length())) {
      bestMatch = iter.first;
      zone = &iter.second;
    }
  }
  return zone;
}

void MapManager::saveRoom(MapRoom* room)
{
  if (!mapFile) {
    qWarning("Cannot save room when no map file is loaded");
    return;
  }

  if (room->zone.isEmpty() && gmcpMode) {
    qWarning() << "No zone in GMCP mode";
    return;
  }

  if (!gmcpMode) {
    mapFile->setValue("autoID", autoRoomId);
  }

  SettingsGroup zoneGroup(mapFile, room->zone.isEmpty() ? "-" : room->zone);
  SettingsGroup roomGroup(mapFile, QString::number(room->id));
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

MapLayout* MapManager::layout()
{
  MapLayout* l = mapLayout.get();
  if (!l) {
    l = new MapLayout(this);
    mapLayout.reset(l);
  }
  return l;
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

QString MapManager::roomType(int roomId) const
{
  return rooms.value(roomId).roomType;
}

int MapManager::roomCost(const QString& roomType) const
{
  return roomCosts.value(roomType, 1);
}

void MapManager::setRoomCost(const QString& roomType, int cost)
{
  if (cost < 1) {
    cost = 1;
  }
  roomCosts[roomType] = cost;
  if (mapFile) {
    mapFile->setValue(QStringLiteral(" RoomTypes/%1/cost").arg(roomType), cost);
  }
}

QColor MapManager::roomColor(int roomId) const
{
  MapRoom room = rooms.value(roomId);
  QColor color = roomColors.value(room.roomType);
  if (!color.isValid() && !room.roomType.isEmpty()) {
    color = colorHeuristic(room.roomType);
  }
  if (!color.isValid()) {
    color = colorHeuristic(room.name);
  }
  return color;
}

QColor MapManager::roomColor(const QString& roomType) const
{
  if (roomColors.contains(roomType)) {
    return roomColors.value(roomType);
  }
  QString check = roomType.toLower().replace(" ", "");
  for (auto [name, color] : cpairs(roomColors)) {
    if (roomType.contains(name.toLower())) {
      return color;
    }
  }
  return colorHeuristic(roomType);
}

void MapManager::setRoomColor(const QString& roomType, const QColor& color)
{
  roomColors[roomType] = color;
  if (mapFile) {
    QString key = QStringLiteral(" RoomTypes/%1/color").arg(roomType);
    if (color.isValid()) {
      mapFile->setValue(key, color.name());
    } else {
      mapFile->remove(key);
    }
  }
}

QStringList MapManager::roomTypes() const
{
  QStringList types = roomCosts.keys();
  for (const QString& key : roomColors.keys()) {
    if (!types.contains(key)) {
      types << key;
    }
  }
  return types;
}

void MapManager::removeRoomType(const QString& roomType)
{
  roomCosts.remove(roomType);
  roomColors.remove(roomType);

  if (mapFile) {
    mapFile->remove(QStringLiteral(" RoomTypes/%1").arg(roomType));
  }
}

QStringList MapManager::routeAvoidZones() const
{
  if (!mapFile) {
    return {};
  }

  QStringList result;
  SettingsGroup sg(mapFile, " Routing/avoid");
  for (const QString& key : mapFile->childKeys()) {
    result << mapFile->value(key).toString();
  }
  return result;
}

void MapManager::setRouteAvoidZones(const QStringList& zones)
{
  if (!mapFile) {
    qWarning() << "No map file to save to";
    return;
  }
  mapFile->remove(" Routing/avoid");
  SettingsGroup sg(mapFile, " Routing/avoid");
  for (auto [index, zone] : enumerate(zones)) {
    mapFile->setValue(QString::number(index), zone);
  }
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
