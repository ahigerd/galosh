#ifndef GALOSH_AUTOMAPPER_H
#define GALOSH_AUTOMAPPER_H

#include <QObject>
class MapManager;
class MapRoom;

class AutoMapper : public QObject
{
Q_OBJECT
public:
  AutoMapper(MapManager* map, QObject* parent = nullptr);

  const MapRoom* currentRoom() const;

public slots:
  void commandEntered(const QString& command, bool echo);
  void processLine(const QString& line);
  void gmcpEvent(const QString& key, const QVariant& value);
  void promptWaiting();

signals:
  void currentRoomUpdated(int roomId);

private slots:
  void reset();

private:
  void endRoomCapture();

  MapManager* map;

  bool logRoomLegacy;
  bool logRoomDescription;
  bool logExits;
  bool roomDirty;
  QString pendingDescription;
  QStringList pendingLines;
  int currentRoomId;
  int destinationRoomId;
  int previousRoomId;
  QString destinationDir;
};

#endif
