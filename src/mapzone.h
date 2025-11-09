#ifndef GALOSH_MAPZONE_H
#define GALOSH_MAPZONE_H

#include <QString>
#include <QList>
#include <QMap>
#include <QSet>
class MapManager;
class MapRoom;

using ZoneID = QString;

class MapZone
{
private:
  MapManager* map;

public:
  using StartRoom = int;
  using DestRoom = int;

  MapZone(MapManager* map, const QString& name);

  ZoneID name;
  QSet<int> roomIds;
  QMap<ZoneID, QSet<int>> exits;
  QMap<DestRoom, QMap<StartRoom, int>> costToExit;

  void addRoom(const MapRoom* room);
  void computeCostsToExits(bool reset = false);
  void computeTransits(bool reset = false);

  QList<int> findRoute(int startRoomId, int endRoomId);
  QList<int> findRoute(int startRoomId, int endRoomId) const;

  QPair<QList<ZoneID>, int> findZoneRoute(const ZoneID& dest, QList<ZoneID> avoid = {}) const;

  static QList<int> findWorldRoute(MapManager* map, int startRoomId, int endRoomId, const QList<ZoneID>& avoid = {});

private:
  QMap<StartRoom, int> getCostsToRoom(int endRoomId, QSet<int>& blocked) const;
  QList<int> findRouteByCost(int startRoomId, int endRoomId, const QMap<int, int>& costs) const;
  QList<int> pickRoute(const QSet<int>& startRoomIds, const QSet<int>& endRoomIds, const QSet<int>& nextRoomIds) const;

  QSet<int> blockedRooms;
};

#endif
