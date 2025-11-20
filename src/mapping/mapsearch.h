#ifndef GALOSH_MAPSEARCH_H
#define GALOSH_MAPSEARCH_H

#include <QSet>
#include <QMultiMap>
#include <QList>
#include <QString>
#include <QPair>
#include <list>
class MapManager;
class MapZone;

class MapSearch
{
public:
  struct Clique;
  struct CliqueExit {
    int fromRoomId;
    Clique* toClique;
    int toRoomId;
  };

  struct Clique {
    const MapZone* zone;
    QSet<int> roomIds;
    QSet<int> unresolvedExits;
    QList<CliqueExit> exits;
    QMap<QPair<int, int>, QList<int>> routes;
  };

  MapSearch(MapManager* map);

  void reset();
  void markDirty(const MapZone* zone = nullptr);
  bool precompute(bool force = false);
  QList<const Clique*> cliquesForZone(const MapZone* zone) const;
  QList<const Clique*> findCliqueRoute(int startRoomId, int endRoomId, const QStringList& avoidZones = {}) const;
  QList<int> findRoute(int startRoomId, int endRoomId, const QStringList& avoidZones = {}) const;

private:
  void getCliquesForZone(const MapZone* zone);
  void crawlClique(Clique* clique, int roomId);
  void resolveExits(Clique* clique);
  void fillRoutes(Clique* clique, int startRoomId);
  QMap<int, int> getCosts(const Clique* clique, int startRoomId, int endRoomId = -1) const;
  QList<int> findRouteInClique(int startRoomId, int endRoomId, const QMap<int, int>& costs, int maxCost = 0) const;
  Clique* newClique(const MapZone* parent);
  Clique* findClique(const QString& zoneName, int roomId) const;
  Clique* findClique(int roomId) const;
  QList<const Clique*> collectCliques(const QStringList& zones) const;
  QPair<QList<const MapSearch::Clique*>, int> findCliqueRoute(const Clique* fromClique, const Clique* toClique, QList<const Clique*> avoid) const;

  struct CliqueStep {
    const Clique* clique;
    QSet<int> endRoomIds;
    QSet<int> nextRoomIds;
  };
  QList<int> findRoute(const QList<CliqueStep>& steps, int index, int startRoomId) const;

  MapManager* map;
  std::list<Clique> cliqueStore;
  QMap<QString, QList<Clique*>> cliques;
  QSet<int> pendingRoomIds;
  QSet<const MapZone*> dirtyZones;
};

#endif
