#ifndef GALOSH_MAPZONE_H
#define GALOSH_MAPZONE_H

#include <QString>
#include <QList>
#include <QMap>
#include <QSet>
class MapManager;
class MapRoom;

using ZoneID = QString;

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
  QSet<int> entrances;

  QString findExit(int dest) const;
  QSet<int> exitRooms() const;
};

class MapZone
{
private:
  MapManager* map;

public:
  MapZone(MapManager* map, const QString& name);

  ZoneID name;
  QSet<int> roomIds;
  QMap<ZoneID, QSet<int>> exits;

  void addRoom(const MapRoom* room);
};

#endif
