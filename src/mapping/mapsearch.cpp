#include "mapsearch.h"
#include "mapmanager.h"
#include "mapzone.h"
#include "algorithms.h"
#include <QtDebug>

MapSearch::MapSearch(MapManager* map)
: map(map)
{
  dirtyZones << nullptr;
}

void MapSearch::reset()
{
  cliques.clear();
  cliqueStore.clear();
  pendingRoomIds.clear();
  dirtyZones << nullptr;
}

void MapSearch::markDirty(const MapZone* zone)
{
  dirtyZones << zone;
}

bool MapSearch::precompute(bool force)
{
  force = force || dirtyZones.contains(nullptr);
  if (force) {
    reset();
  } else {
    for (const MapZone* zone : dirtyZones) {
      auto zoneCliques = cliques.take(zone->name);
      for (Clique* clique : zoneCliques) {
        for (auto iter = cliqueStore.begin(); iter != cliqueStore.end(); iter++) {
          if (&*iter == clique) {
            cliqueStore.erase(iter);
            break;
          }
        }
      }
    }
  }

  for (const QString& zoneName : map->zoneNames()) {
    getCliquesForZone(map->zone(zoneName));
  }
  for (Clique& clique : cliqueStore) {
    resolveExits(&clique);
    QSet<int> distinctExits;
    for (const CliqueExit& exit : clique.exits) {
      distinctExits << exit.fromRoomId;
    }
    int n = distinctExits.size();
    if (force || (n * (n - 1)) != clique.routes.size()) {
      for (const CliqueExit& exit : clique.exits) {
        fillRoutes(&clique, exit.fromRoomId);
      }
      clique.unresolvedExits.clear();
    }
  }

  dirtyZones.clear();
  return true;
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
  if (cliques.contains(zone->name)) {
    // already computed
    return;
  }

  QSet<int> allExits;
  for (const QSet<int>& exitGroup : zone->exits) {
    allExits += exitGroup;
  }

  pendingRoomIds = zone->roomIds;

  for (int exit : allExits) {
    if (!pendingRoomIds.contains(exit)) {
      // already part of a clique
      continue;
    }
    Clique* clique = newClique(zone);
    pendingRoomIds.remove(exit);
    clique->roomIds << exit;
    crawlClique(clique, exit);
  }

  // operate on a copy because it can get modified
  for (int roomId : QSet<int>(pendingRoomIds)) {
    if (!pendingRoomIds.contains(roomId)) {
      // removed by another call
      continue;
    }
    Clique* clique = newClique(zone);
    clique->roomIds << roomId;
    crawlClique(clique, roomId);

    // Check for clique intersections and merge if needed
    for (Clique* other : QList<Clique*>(cliques[zone->name])) {
      if (clique == other || !clique->roomIds.intersects(other->roomIds)) {
        continue;
      }
      other->roomIds.unite(clique->roomIds);
      other->unresolvedExits.unite(clique->unresolvedExits);
      for (const CliqueExit& exit : clique->exits) {
        bool found = false;
        for (const CliqueExit& oexit : other->exits) {
          if (exit.fromRoomId == oexit.fromRoomId && exit.toRoomId == oexit.toRoomId) {
            found = true;
            break;
          }
        }
        if (!found) {
          other->exits << exit;
        }
      }
      cliques[zone->name].removeAll(clique);
      for (auto iter = cliqueStore.begin(); iter != cliqueStore.end(); iter++) {
        if (&*iter == clique) {
          cliqueStore.erase(iter);
          break;
        }
      }
    }
  }
}

QList<const MapSearch::Clique*> MapSearch::cliquesForZone(const MapZone* zone) const
{
  QList<const Clique*> result;
  for (Clique* clique : cliques.value(zone->name)) {
    result << clique;
  }
  return result;
}

void MapSearch::crawlClique(Clique* clique, int roomId)
{
  const MapRoom* room = map->room(roomId);
  if (!room) {
    // invalid room
    return;
  }
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
      pendingRoomIds.remove(exit);
      crawlClique(clique, exit);
    } else if (Clique* destClique = findClique(dest->zone, exit)) {
      clique->exits << CliqueExit{ roomId, destClique, exit };
    } else {
      clique->unresolvedExits << roomId;
    }
  }
  // Check for rooms that connect to this clique that we missed
  // (perhaps because of one-way connections)
  bool extended = true;
  while (extended) {
    extended = false;
    // Iterate over a copy
    for (int pendingId : QSet<int>(pendingRoomIds)) {
      if (!pendingRoomIds.contains(pendingId)) {
        // removed by a recursive call
        continue;
      }
      const MapRoom* room = map->room(pendingId);
      if (!room) {
        continue;
      }
      for (const MapExit& exit : room->exits) {
        if (!clique->roomIds.contains(exit.dest)) {
          // this exit doesn't connect to anything in the clique
          continue;
        }
        const MapRoom* dest = map->room(exit.dest);
        if (!dest || dest->zone != clique->zone->name) {
          continue;
        }
        extended = true;
        clique->roomIds << pendingId;
        pendingRoomIds.remove(pendingId);
      }
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

MapSearch::Clique* MapSearch::findClique(int roomId) const
{
  const MapRoom* room = map->room(roomId);
  if (!room) {
    return nullptr;
  }
  return findClique(room->zone, roomId);
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
      if (!room) {
        continue;
      }
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
  return costs;
}

QList<int> MapSearch::findRouteInClique(int startRoomId, int endRoomId, const QMap<int, int>& costs, int maxCost) const
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
    QList<int> route = findRouteInClique(startRoomId, nextRoomId, costs, costs[nextRoomId]);
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
  bestRoute << endRoomId;
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
      // already have this route
      continue;
    }
    if (!costs.contains(endRoomId)) {
      // Can't get there from here -- perhaps one-way connection
      continue;
    }
    clique->routes[key] = findRouteInClique(startRoomId, endRoomId, costs);
  }
}

QList<const MapSearch::Clique*> MapSearch::findCliqueRoute(int startRoomId, int endRoomId, const QStringList& avoidZones) const
{
  const Clique* fromClique = findClique(startRoomId);
  const Clique* toClique = findClique(endRoomId);
  auto [path, cost] = findCliqueRoute(fromClique, toClique, collectCliques(avoidZones));
  return path;
}

QList<const MapSearch::Clique*> MapSearch::collectCliques(const QStringList& zones) const
{
  QList<const Clique*> result;
  for (const QString& zone : zones) {
    for (Clique* clique : cliques.value(zone)) {
      result << clique;
    }
  }
  return result;
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
    if (cost <= 0 || path.length() < 1) {
      // cost == 0 would imply a direct connection
      // but that should have been caught by transits
      continue;
    }
    const Clique* transitTo = path[0];
    int transitCost = 0;
    for (const CliqueExit& nextExit : next->exits) {
      if (nextExit.toClique == transitTo) {
        QPair<int, int> key(exit.toRoomId, nextExit.fromRoomId);
        int thisTransitCost = next->routes.value(key).length();
        if (thisTransitCost > 0 && (!transitCost || thisTransitCost < transitCost)) {
          transitCost = thisTransitCost;
        }
      }
    }
    if (!transitCost) {
      continue;
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

QList<int> MapSearch::findRoute(int startRoomId, int endRoomId, const QStringList& avoidZones) const
{
  const Clique* startClique = findClique(startRoomId);
  const Clique* endClique = findClique(endRoomId);
  if (!startClique || !endClique) {
    return {};
  }

  if (startClique == endClique) {
    QMap<int, int> costs = getCosts(startClique, startRoomId, endRoomId);
    return findRouteInClique(startRoomId, endRoomId, costs);
  }

  QList<const Clique*> avoidCliques = collectCliques(avoidZones);
  auto [cliqueRoute, cost] = findCliqueRoute(startClique, endClique, avoidCliques);
  if (cliqueRoute.isEmpty()) {
    return {};
  }
  cliqueRoute.insert(0, startClique);
  Q_ASSERT(cliqueRoute.last() == endClique);

  QList<CliqueStep> steps;
  int maxRoute = cliqueRoute.size() - 1;
  for (auto [ i, clique ] : enumerate(cliqueRoute)) {
    if (i == maxRoute) {
      break;
    }
    QSet<int> outRooms, nextRooms;
    for (const CliqueExit& exit : clique->exits) {
      if (i < maxRoute && exit.toClique == cliqueRoute[i + 1]) {
        outRooms << exit.fromRoomId;
        nextRooms << exit.toRoomId;
      }
    }
    steps << CliqueStep{ clique, outRooms, nextRooms };
  }
  steps << CliqueStep{ cliqueRoute.last(), { endRoomId }, {} };

  return findRoute(steps, 0, startRoomId);
}

QList<int> MapSearch::findRoute(const QList<MapSearch::CliqueStep>& steps, int index, int startRoomId) const
{
  const auto& step = steps[index];
  QMap<int, int> costs;
  if (index == 0) {
    costs = getCosts(step.clique, startRoomId);
  } else if (index == steps.length() - 1) {
    for (int endRoomId : step.endRoomIds) {
      costs = getCosts(step.clique, startRoomId, endRoomId);
      break;
    }
  }
  QList<int> bestRoute, stepRoute;
  for (int endRoomId : step.endRoomIds) {
    if (index == 0 || index == steps.length() - 1) {
      stepRoute = findRouteInClique(startRoomId, endRoomId, costs);
    } else {
      stepRoute = step.clique->routes[qMakePair(startRoomId, endRoomId)];
    }
    if (stepRoute.isEmpty()) {
      continue;
    }
    if (step.nextRoomIds.isEmpty()) {
      return stepRoute;
    }
    const MapRoom* endRoom = map->room(endRoomId);
    QList<int> bestNext;
    for (const MapExit& exit : endRoom->exits) {
      if (step.nextRoomIds.contains(exit.dest)) {
        QList<int> nextRoute = findRoute(steps, index + 1, exit.dest);
        if (nextRoute.isEmpty()) {
          continue;
        }
        if (bestNext.isEmpty() || bestNext.length() > nextRoute.length()) {
          bestNext = nextRoute;
        }
      }
    }
    if (bestNext.isEmpty()) {
      continue;
    }
    Q_ASSERT(!bestNext.isEmpty());
    if (bestRoute.isEmpty() || stepRoute.length() + bestNext.length() < bestRoute.length()) {
      bestRoute = stepRoute + bestNext;
    }
  }
  return bestRoute;
}
