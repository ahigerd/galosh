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
  void precompute();
  QList<const MapSearch::Clique*> findCliqueRoute(int startRoomId, int endRoomId) const;

private:
  void getCliquesForZone(const MapZone* zone);
  void crawlClique(Clique* clique, int roomId);
  void resolveExits(Clique* clique);
  void fillRoutes(Clique* clique, int startRoomId);
  QMap<int, int> getCosts(const Clique* clique, int startRoomId, int endRoomId = -1) const;
  QList<int> findRouteByCost(int startRoomId, int endRoomId, const QMap<int, int>& costs, int maxCost = 0) const;
  Clique* newClique(const MapZone* parent);
  Clique* findClique(const QString& zoneName, int roomId) const;
  QPair<QList<const MapSearch::Clique*>, int> findCliqueRoute(const Clique* fromClique, const Clique* toClique, QList<const Clique*> avoid) const;

  MapManager* map;
  std::list<Clique> cliqueStore;
  QMap<QString, QList<Clique*>> cliques;
};

#endif
