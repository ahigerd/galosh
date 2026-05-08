#ifndef GALOSH_MAPSEARCH_H
#define GALOSH_MAPSEARCH_H

#include <QObject>
#include <QSet>
#include <QMultiMap>
#include <QList>
#include <QString>
#include <QRect>
#include <QPair>
#include <list>
#include "refable.h"
class MapManager;
class MapZone;
class MapRoom;

class MapSearch : public QObject
{
Q_OBJECT
public:
  struct Clique;

  struct Route {
    QList<int> rooms;
    int cost;
  };

  struct Grid {
    MapManager* map;
    QVector<QVector<int>> grid;
    QSet<int> rooms;

    bool mask(const QSet<int>& exclude);

    QString debug(const QSet<int>& exclude = {}) const;
    bool operator<(const Grid& other) const;
    operator bool() const;

    QPair<int, int> pos(int roomId) const;
    int at(int x, int y) const;
    inline int at(const QPair<int, int>& pos) const { return at(pos.first, pos.second); }

    inline int size() const { return rooms.size(); }
    inline bool contains(int roomId) const { return rooms.contains(roomId); }
    inline bool contains(const Grid& other) const { return rooms.contains(other.rooms); }
  };

  struct CliqueExit;
  struct Clique : public Refable<Clique> {
    const MapZone* zone;
    QSet<int> roomIds;
    QList<CliqueExit> exits;
    QMap<QPair<int, int>, Route> routes;
    QList<Grid> grids;
  };

  struct CliqueExit {
    int fromRoomId;
    Clique::MRef toClique;
    int toRoomId;
  };

  struct CliqueRoute {
    QList<Clique::Ref> cliques;
    int cost;
  };

  MapSearch(MapManager* map);

  void reset();
  void markDirty(const MapZone* zone = nullptr);
  bool precompute(bool force = false);
  QList<Clique::Ref> cliquesForZone(const MapZone* zone) const;
  QList<Clique::Ref> findCliqueRoute(int startRoomId, int endRoomId, const QStringList& avoidZones = {}) const;
  QList<int> findRoute(int startRoomId, int endRoomId, const QStringList& avoidZones = {}) const;
  QList<int> findRoute(int startRoomId, const QString& destZone, const QStringList& avoidZones = {}) const;
  QStringList routeDirections(const QList<int>& route) const;

private:
  void getCliquesForZone(const MapZone* zone);
  void crawlClique(Clique::MRefR clique, int roomId);
  void resolveExits(Clique::MRefR clique);
  QMap<int, int> getCosts(Clique::RefR clique, int startRoomId, int endRoomId = -1) const;
  Clique::MRef newClique(const MapZone* parent);
  Clique::MRef findClique(const QString& zoneName, int roomId) const;
  Clique::MRef findClique(int roomId) const;
  QList<Clique::Ref> collectCliques(const QStringList& zones) const;

  struct Node {
    int roomId = -1;
    int cost;
    QVector<int> exits;
    QVector<int> entrances;
  };
  QMap<int, Node> nodes;
  void generateNodes(const MapRoom* start);
  QHash<int, int> costsFromNode(int startRoomId, bool reverse, const QSet<int>& avoidRooms = {}) const;
  QPair<QList<int>, int> findRoute(int startRoomId, int endRoomId, const QSet<int>& avoidRooms) const;

public:
  MapManager* map;
  std::list<Clique> cliqueStore;
  QMap<QString, QList<Clique::MRef>> cliques;
  QSet<int> pendingRoomIds;
  QSet<const MapZone*> dirtyZones;
};

#endif
