#ifndef GALOSH_MAPMANAGER_H
#define GALOSH_MAPMANAGER_H

#include <QObject>
#include <QList>
#include <QRegularExpression>
class QSettings;

struct MapExit {
  QString name;
  int dest;
  bool door;
  bool open;
  bool locked;
  bool lockable;
};

struct MapRoom {
  int id;
  QString name;
  QString description;
  QString zone;
  QString roomType;
  QMap<QString, MapExit> exits;
};

class MapManager : public QObject
{
Q_OBJECT
public:
  MapManager(QObject* parent = nullptr);

  const MapRoom* room(int id);
  MapRoom* mutableRoom(int id);
  void saveRoom(MapRoom* room);

signals:
  void currentRoomUpdated(MapManager* map, int roomId);

public slots:
  void loadProfile(const QString& profile);
  void loadMap(const QString& filename);
  void processLine(const QString& line);
  void msspEvent(const QString& key, const QString& value);
  void gmcpEvent(const QString& key, const QVariant& value);

private:
  void downloadMap(const QString& url);
  void updateRoom(const QVariantMap& info);

  QSettings* mapFile;
  QMap<int, MapRoom> rooms;
  int currentRoom;
  bool logRoomDescription;
  bool logExits;
  bool roomDirty;
  QString pendingDescription;
};

#endif
