#ifndef GALOSH_MAPMANAGER_H
#define GALOSH_MAPMANAGER_H

#include <QObject>
#include <QList>
#include <QSet>
#include <QRegularExpression>
#include <memory>
#include <map>
#include "mapzone.h"
#include "mapsearch.h"
class QSettings;

class MapManager : public QObject
{
Q_OBJECT
public:
  MapManager(QObject* parent = nullptr);

  const MapRoom* room();
  const MapRoom* room(int id);
  MapRoom* mutableRoom(int id);
  QList<const MapRoom*> searchForRooms(const QStringList& args, bool namesOnly, const QString& zone = QString()) const;
  void saveRoom(MapRoom* room);

  QStringList zoneNames() const;
  const MapZone* zone(const QString& name) const;
  MapZone* mutableZone(const QString& name);
  const MapZone* searchForZone(const QString& name) const;

  MapSearch* search();

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

  std::unique_ptr<MapSearch> mapSearch;
};

#endif
