#ifndef GALOSH_MAPMANAGER_H
#define GALOSH_MAPMANAGER_H

#include <QObject>
#include <QList>
#include <QSet>
#include <QRegularExpression>
#include <map>
#include "mapzone.h"
class QSettings;
class MapManager;

struct MapExit {
  QString name;
  int dest = -1;
  bool door = false;
  bool open = false;
  bool locked = false;
  bool lockable = false;
};

struct MapRoom {
  static QString normalizeDir(const QString& dir);
  static QString reverseDir(const QString& dir);
  static bool isDir(const QString& dir);

  int id;
  QString name;
  QString description;
  QString zone;
  QString roomType;
  QMap<QString, MapExit> exits;

  QString findExit(int dest) const;
  QSet<int> exitRooms() const;
};

class MapManager : public QObject
{
Q_OBJECT
public:
  MapManager(QObject* parent = nullptr);

  const MapRoom* room(int id);
  MapRoom* mutableRoom(int id);
  void saveRoom(MapRoom* room);

  const MapZone* zone(const QString& name) const;
  MapZone* mutableZone(const QString& name);

signals:
  void currentRoomUpdated(MapManager* map, int roomId);

public slots:
  void loadProfile(const QString& profile);
  void loadMap(const QString& filename);
  void processLine(const QString& line);
  void gmcpEvent(const QString& key, const QVariant& value);
  void promptWaiting();
  void commandEntered(const QString& command, bool echo);

private:
  friend class MapZone;
  void downloadMap(const QString& url);
  void updateRoom(const QVariantMap& info);
  void endRoomCapture();

  QSettings* mapFile;
  QMap<int, MapRoom> rooms;
  std::map<QString, MapZone> zones;
  QMap<QString, QSet<QString>> zoneConnections;
  QMap<QString, QMap<QString, QMap<QString, int>>> zoneTransits;
  bool gmcpMode;
  bool logRoomLegacy;
  bool logRoomDescription;
  bool logExits;
  bool roomDirty;
  QString pendingDescription;
  QStringList pendingLines;
  int autoRoomId;
  int currentRoom;
  int destinationRoom;
  int previousRoom;
  QString destinationDir;
};

#endif
