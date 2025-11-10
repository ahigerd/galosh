#include "mapsearch.h"
#include "mapmanager.h"
#include "mapzone.h"
#include <QtDebug>

MapSearch::MapSearch(MapManager* map)
: map(map)
{
  // initializers only
}

void MapSearch::reset()
{
  cliques.clear();
  cliqueStore.clear();
}

void MapSearch::precompute()
{
  for (const QString& zone : map->zoneNames()) {
    getCliquesForZone(map->zone(zone));
  }
  for (Clique& clique : cliqueStore) {
    resolveExits(&clique);
    for (const CliqueExit& exit : clique.exits) {
      fillRoutes(&clique, exit.fromRoomId);
    }
  }
}

MapSearch::Clique* MapSearch::newClique(const MapZone* parent)
{
  cliqueStore.push_back(Clique());
  Clique* clique = &cliqueStore.back();
  clique->zone = parent;
  cliques[parent->name] << clique;
  return clique;
}

void MapSearch::getCliquesForZone(const MapZone* zone)
{
  QList<Clique*>& zoneCliques = cliques[zone->name];

  QSet<int> allExits;
  for (const QSet<int>& exitGroup : zone->exits) {
    allExits += exitGroup;
  }

  for (int exit : allExits) {
    bool done = false;
    for (Clique* clique : zoneCliques) {
      if (clique->roomIds.contains(exit)) {
        done = true;
        break;
      }
    }
    if (done) {
      // exit is already part of a clique
      continue;
    }
    Clique* clique = newClique(zone);
    clique->roomIds << exit;
    crawlClique(clique, exit);
  }
  for (int roomId : zone->roomIds) {
    if (!findClique(zone->name, roomId)) {
      Clique* clique = newClique(zone);
      clique->roomIds << roomId;
      crawlClique(clique, roomId);
    }
  }
}

void MapSearch::crawlClique(Clique* clique, int roomId)
{
  const MapRoom* room = map->room(roomId);
  for (int exit : room->exitRooms()) {
    const MapRoom* dest = map->room(exit);
    if (!dest) {
      continue;
    }
    if (dest->zone == clique->zone->name) {
      if (clique->roomIds.contains(exit)) {
        continue;
      }
      clique->roomIds << exit;
      crawlClique(clique, exit);
    } else if (Clique* destClique = findClique(dest->zone, exit)) {
      clique->exits << CliqueExit{ roomId, destClique, exit };
    } else {
      clique->unresolvedExits << roomId;
    }
  }
}

void MapSearch::resolveExits(MapSearch::Clique* clique)
{
  for (int exit : clique->unresolvedExits) {
    const MapRoom* room = map->room(exit);
    if (!room) {
      continue;
    }
    for (int destId : room->exitRooms()) {
      const MapRoom* dest = map->room(destId);
      if (!dest) {
        continue;
      }
      Clique* destClique = findClique(dest->zone, destId);
      if (destClique) {
        clique->exits << CliqueExit{ exit, destClique, destId };
      }
    }
  }
  clique->unresolvedExits.clear();
}

MapSearch::Clique* MapSearch::findClique(const QString& zoneName, int roomId) const
{
  for (Clique* clique : cliques.value(zoneName)) {
    if (clique->roomIds.contains(roomId)) {
      return clique;
    }
  }
  return nullptr;
}

QMap<int, int> MapSearch::getCosts(const MapSearch::Clique* clique, int startRoomId, int endRoomId) const
{
  QMap<int, int> costs;
  costs[startRoomId] = 1;

  QSet<int> destsRemaining;
  if (endRoomId < 0) {
    for (const CliqueExit& exit : clique->exits) {
      if (exit.fromRoomId == startRoomId) {
        continue;
      }
      destsRemaining << exit.fromRoomId;
    }
    if (destsRemaining.isEmpty()) {
      // dead-end zone
      return {};
    }
  } else {
    destsRemaining << endRoomId;
  }

  QList<const MapRoom*> frontier({ map->room(startRoomId) });
  QList<const MapRoom*> nextFrontier;
  int round = 2;
  while (!frontier.isEmpty()) {
    for (const MapRoom* room : frontier) {
      for (const MapExit& exit : room->exits) {
        if (costs.contains(exit.dest) || !clique->roomIds.contains(exit.dest)) {
          continue;
        }
        costs[exit.dest] = round;
        nextFrontier << map->room(exit.dest);
        destsRemaining.remove(exit.dest);
        if (destsRemaining.isEmpty()) {
          return costs;
        }
      }
    }
    frontier = nextFrontier;
    nextFrontier.clear();
    round++;
  }
  qWarning() << "XXX: clique is not a clique in getCosts()" << destsRemaining << clique->zone->name;
  return costs;
}

QList<int> MapSearch::findRouteByCost(int startRoomId, int endRoomId, const QMap<int, int>& costs, int maxCost) const
{
  if (startRoomId == endRoomId) {
    return { endRoomId };
  }
  if (!costs.contains(startRoomId)) {
    // can't get there from here
    return {};
  }
  if (maxCost <= 0) {
    maxCost = costs[endRoomId];
  }
  const MapRoom* room = map->room(endRoomId);
  if (room->exitRooms().contains(startRoomId)) {
    return { startRoomId, endRoomId };
  }
  QList<int> bestRoute;
  for (int nextRoomId : room->entrances) {
    if (nextRoomId == startRoomId || costs.value(nextRoomId, maxCost) >= maxCost) {
      continue;
    }
    QList<int> route = findRouteByCost(startRoomId, nextRoomId, costs, costs[nextRoomId]);
    if (route.isEmpty()) {
      continue;
    }
    if (bestRoute.isEmpty() || bestRoute.length() > route.length()) {
      bestRoute = route;
    }
  }
  if (bestRoute.isEmpty()) {
    return {};
  }
  bestRoute.insert(0, startRoomId);
  return bestRoute;
}

void MapSearch::fillRoutes(MapSearch::Clique* clique, int startRoomId)
{
  QMap<int, int> costs = getCosts(clique, startRoomId);
  if (costs.isEmpty()) {
    return;
  }
  Q_ASSERT(costs.contains(startRoomId));

  for (const CliqueExit& exit : clique->exits) {
    int endRoomId = exit.fromRoomId;
    if (endRoomId == startRoomId) {
      continue;
    }
    QPair<int, int> key(startRoomId, endRoomId);
    if (clique->routes.contains(key)) {
      continue;
    }
    Q_ASSERT(costs.contains(endRoomId));
    clique->routes[key] = findRouteByCost(startRoomId, endRoomId, costs);
  }
}

QStringList debug(const QList<const MapSearch::Clique*>& path)
{
  QStringList rv;
  for (auto c : path) rv << c->zone->name;
  return rv;
}

QList<const MapSearch::Clique*> MapSearch::findCliqueRoute(int startRoomId, int endRoomId) const
{
  const MapZone* startZone = map->zone(map->room(startRoomId)->zone);
  const MapZone* endZone = map->zone(map->room(endRoomId)->zone);
  qDebug() << startZone->name << endZone->name;
  const Clique* fromClique = findClique(startZone->name, startRoomId);
  const Clique* toClique = findClique(endZone->name, endRoomId);
  auto [path, cost] = findCliqueRoute(fromClique, toClique, {});
  qDebug() << debug(path) << cost;
  return path;
}

QPair<QList<const MapSearch::Clique*>, int> MapSearch::findCliqueRoute(const MapSearch::Clique* fromClique, const MapSearch::Clique* toClique, QList<const MapSearch::Clique*> avoid) const
{
  // Step 1: direct connection
  for (const CliqueExit& exit : fromClique->exits) {
    if (exit.toClique == toClique) {
      return { { toClique }, 1 };
    }
  }
  // Step 2: single transit
  for (const Clique& through : cliqueStore) {
    int foundFrom = -1;
    int foundTo = -1;
    for (const CliqueExit& exit : through.exits) {
      if (exit.toClique == fromClique) {
        foundFrom = exit.fromRoomId;
      }
      if (exit.toClique == toClique) {
        foundTo = exit.fromRoomId;
      }
    }
    if (foundFrom > 0 && foundTo > 0) {
      auto path = through.routes[qMakePair(foundFrom, foundTo)];
      return { { &through, toClique }, through.routes[qMakePair(foundFrom, foundTo)].length() };
    }
  }
  // Step 3: recursive search
  avoid << fromClique;
  int bestCost = 0;
  QList<const Clique*> bestPath;
  QStringList avoidStr;
  for (auto a : avoid) avoidStr << a->zone->name;

  for (const CliqueExit& exit : fromClique->exits) {
    const Clique* next = exit.toClique;
    if (avoid.contains(next)) {
      continue;
    }
    auto [path, cost] = findCliqueRoute(next, toClique, avoid);
    if (cost <= 0 || path.length() < 2) {
      // cost == 0 would imply a direct connection
      // but that should have been caught by transits
      continue;
    }
    const Clique* through = path[0];
    const Clique* transitTo = path[1];
    int transitCost = 0;
    for (const CliqueExit& throughExit : through->exits) {
      if (throughExit.toClique == transitTo) {
        QPair<int, int> key(exit.toRoomId, throughExit.fromRoomId);
        int thisTransitCost = through->routes.value(key).length();
        if (thisTransitCost > 0 && (!transitCost || thisTransitCost < transitCost)) {
          transitCost = thisTransitCost;
        }
      }
    }
    cost += transitCost;
    if (!bestCost || cost < bestCost) {
      bestPath = path;
      bestCost = cost;
      bestPath.insert(0, next);
    }
  }
  if (bestPath.isEmpty()) {
    return { {}, -1 };
  }
  return { bestPath, bestCost };
}
