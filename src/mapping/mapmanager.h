#ifndef GALOSH_MAPMANAGER_H
#define GALOSH_MAPMANAGER_H

#include <QObject>
#include <QList>
#include <QSet>
#include <QRegularExpression>
#include <QColor>
#include <memory>
#include <map>
#include "mapzone.h"
#include "mapsearch.h"
#include "maplayout.h"
class QSettings;

class MapManager : public QObject
{
Q_OBJECT
public:
  static QString mapForProfile(const QString& profile);
  static QColor colorHeuristic(const QString& roomType);

  MapManager(QObject* parent = nullptr);

  const MapRoom* room(int id) const;
  MapRoom* mutableRoom(int id);
  QList<const MapRoom*> searchForRooms(const QStringList& args, bool namesOnly, const QString& zone = QString()) const;
  void saveRoom(MapRoom* room);

  QStringList zoneNames() const;
  const MapZone* zone(const QString& name) const;
  MapZone* mutableZone(const QString& name);
  const MapZone* searchForZone(const QString& name) const;

  MapSearch* search();
  MapLayout* layout();

  int waypoint(const QString& name, QString* canonicalName = nullptr) const;
  QStringList waypoints() const;
  bool setWaypoint(const QString& name, int roomId);
  bool removeWaypoint(const QString& name);

  QStringList roomTypes() const;
  QString roomType(int roomId) const;
  inline QString roomType(const MapRoom* room) const { return room ? room->roomType : QString(); }
  void removeRoomType(const QString& roomType);

  inline int roomCost(int roomId) const { return roomCost(roomType(roomId)); }
  inline int roomCost(const MapRoom* room) const { return roomCost(roomType(room)); }
  int roomCost(const QString& roomType) const;
  void setRoomCost(const QString& roomType, int cost);

  QColor roomColor(int roomId) const;
  inline QColor roomColor(const MapRoom* room) const { return roomColor(roomType(room)); }
  QColor roomColor(const QString& roomType) const;
  void setRoomColor(const QString& roomType, const QColor& color);

  QStringList routeAvoidZones() const;
  void setRouteAvoidZones(const QStringList& zones);

  QSettings* mapProfile() const;

signals:
  void roomUpdated(int roomId);
  void reset();

public slots:
  void loadProfile(const QString& profile);
  void loadMap(const QString& filename);

private:
  friend class AutoMapper;
  friend class MapZone;
  void downloadMap(const QString& url);
  void updateRoom(const QVariantMap& info);

  QSettings* mapFile;
  QMap<QString, int> roomCosts;
  QMap<QString, QColor> roomColors;
  QMap<int, MapRoom> rooms;
  std::map<QString, MapZone> zones;
  bool gmcpMode;
  int autoRoomId;

  std::unique_ptr<MapSearch> mapSearch;
  std::unique_ptr<MapLayout> mapLayout;
};

#endif
