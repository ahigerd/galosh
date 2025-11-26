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

  struct CliqueRoute {
    QList<const Clique*> cliques;
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

  struct CliqueExit {
    int fromRoomId;
    Clique* toClique;
    int toRoomId;
  };

  struct Clique {
    const MapZone* zone;
    QSet<int> roomIds;
    QList<CliqueExit> exits;
    QMap<QPair<int, int>, Route> routes;
    QList<Grid> grids;
  };

  MapSearch(MapManager* map);

  void reset();
  void markDirty(const MapZone* zone = nullptr);
  bool precompute(bool force = false);
  void precomputeRoutes();
  QList<const Clique*> cliquesForZone(const MapZone* zone) const;
  QList<const Clique*> findCliqueRoute(int startRoomId, int endRoomId, const QStringList& avoidZones = {}) const;
  QList<int> findRoute(int startRoomId, int endRoomId, const QStringList& avoidZones = {}) const;
  QStringList routeDirections(const QList<int>& route) const;

private slots:
  void precomputeIncremental();

private:
  void getCliquesForZone(const MapZone* zone);
  void crawlClique(Clique* clique, int roomId);
  void resolveExits(Clique* clique);
  void findGrids(Clique* clique);
  void fillRoutes(Clique* clique, int startRoomId);
  QMap<int, int> getCosts(const Clique* clique, int startRoomId, int endRoomId = -1) const;
  Route findRouteInClique(const Clique* clique, int startRoomId, int endRoomId, const QMap<int, int>& costs, int maxCost = 0) const;
  Clique* newClique(const MapZone* parent);
  Clique* findClique(const QString& zoneName, int roomId) const;
  Clique* findClique(int roomId) const;
  QList<const Clique*> collectCliques(const QStringList& zones) const;
  CliqueRoute findCliqueRoute(const Clique* fromClique, const Clique* toClique, QList<const Clique*> avoid) const;

  struct CliqueStep {
    const Clique* clique;
    QSet<int> endRoomIds;
    QSet<int> nextRoomIds;
  };
  Route findRoute(const QList<CliqueStep>& steps, int index, int startRoomId) const;

public:
  MapManager* map;
  std::list<Clique> cliqueStore;
  QMap<QString, QList<Clique*>> cliques;
  QSet<const Clique*> deadEnds;
  QSet<int> pendingRoomIds;
  QSet<const MapZone*> dirtyZones;
  std::list<QPair<Clique*, int>> incremental;
};

#endif
