#ifndef GALOSH_MAPMANAGER_H
#define GALOSH_MAPMANAGER_H

#include <QObject>
#include <QList>
#include <QRegularExpression>

struct MapExit {
  QString name;
  int dest;
  bool door;
  bool open;
  bool locked;
};

struct MapRoom {
  QString name;
  QString description;
  QMap<QString, MapExit> exits;
};

class MapManager : public QObject
{
Q_OBJECT
public:
  MapManager(QObject* parent = nullptr);

  const MapRoom* room(int id);

signals:
  void currentRoomUpdated(MapManager* map, int roomId);

public slots:
  void loadProfile(const QString& profile);
  void processLine(const QString& line);
  void msspEvent(const QString& key, const QString& value);
  void gmcpEvent(const QString& key, const QVariant& value);

private:
  void updateRoom(const QVariantMap& info);

  QMap<int, MapRoom> rooms;
  int currentRoom;
  bool logRoomDescription;
  bool logExits;
  bool roomDirty;
  QString pendingDescription;
};

#endif
