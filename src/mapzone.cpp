#include "mapzone.h"
#include "mapmanager.h"
#include <limits>
#include <QtDebug>

static bool debug = false;

MapZone::MapZone(MapManager* map, const QString& name)
: map(map), name(name)
{
  // initializers only
}

void MapZone::addRoom(const MapRoom* room)
{
  roomIds << room->id;
  for (const MapExit& exit : room->exits) {
    const MapRoom* dest = map->room(exit.dest);
    if (dest && dest->zone != name && !exits.value(dest->zone).contains(room->id)) {
      exits[dest->zone] << room->id;
      map->zoneConnections[name] << dest->zone;

      // Check for a reverse link too
      MapZone* otherZone = map->mutableZone(dest->zone);
      otherZone->addRoom(dest);
    }
  }
}

void MapZone::computeTransits(bool reset)
{
  computeCostsToExits(reset);

  QSet<ZoneID> direct = map->zoneConnections[name];
  for (const ZoneID& fromZone : exits.keys()) {
    if (!reset && map->zoneTransits.value(fromZone).contains(name)) {
      // cached
      continue;
    }
    for (const ZoneID& toZone : exits.keys()) {
      if (fromZone == toZone) {
        continue;
      }
      int costSum = 0;
      int numRoutes = 0;
      for (int start : exits[fromZone]) {
        for (int end : exits[toZone]) {
          auto route = findRouteByCost(start, end, costToExit[end]);
          if (route.isEmpty()) {
            continue;
          }
          ++numRoutes;
          costSum += route.length();
        }
      }
      if (numRoutes > 0) {
        // track the average cost
        map->zoneTransits[fromZone][name][toZone] = 1 + costSum / numRoutes;
      }
    }
  }
}

void MapZone::computeCostsToExits(bool reset)
{
  if (!reset && !costToExit.isEmpty()) {
    return;
  }
  QSet<int> allExits;
  for (const QSet<int>& exitSet : exits) {
    allExits += exitSet;
  }
  for (int endRoomId : allExits) {
    costToExit[endRoomId] = getCostsToRoom(endRoomId, blockedRooms);
  }
}

QMap<int, int> MapZone::getCostsToRoom(int endRoomId, QSet<int>& blocked) const
{
  QMap<int, int> costs;
  costs[endRoomId] = 1;
  QList<const MapRoom*> frontier({ map->room(endRoomId) });
  QList<const MapRoom*> nextFrontier;
  int round = 1;
  while (!frontier.isEmpty()) {
    for (const MapRoom* room : frontier) {
      for (const MapExit& exit : room->exits) {
        if (costs.contains(exit.dest) || blocked.contains(exit.dest)) {
          continue;
        }
        const MapRoom* next = map->room(exit.dest);
        if (!next || next->zone != name) {
          blocked << exit.dest;
          continue;
        }
        costs[exit.dest] = round;
        nextFrontier << next;
      }
    }
    frontier = nextFrontier;
    nextFrontier.clear();
    round++;
  }
  return costs;
}

QList<int> MapZone::findRouteByCost(int startRoomId, int endRoomId, const QMap<int, int>& costs) const
{
  if (startRoomId == endRoomId) {
    return { endRoomId };
  }
  if (!costs.contains(startRoomId)) {
    // can't get there from here
    return {};
  }
  int progress = std::numeric_limits<int>::max();
  QList<int> path({ startRoomId });
  QMap<int, QSet<int>> failed;
  int currentRoomId = startRoomId;
  while (currentRoomId != endRoomId) {
    int nextRoomId = -1;
    const MapRoom* room = map->room(currentRoomId);
    if (!room) {
      // undefined room
      qDebug() << "XXX: undefined room" << currentRoomId;
      return {};
    }
    if (debug) {
      QList<int> ex;
      for (const MapExit& exit : room->exits) {
        ex << exit.dest;
      }
    }
    for (const MapExit& exit : room->exits) {
      if (exit.dest == endRoomId) {
        nextRoomId = endRoomId;
        break;
      }
      if (blockedRooms.contains(exit.dest) || failed[currentRoomId].contains(exit.dest)) {
        continue;
      }
      int nextCost = costs.value(exit.dest, -1);
      if (nextCost >= 0 && nextCost < progress) {
        nextRoomId = exit.dest;
        progress = nextCost;
      }
    }
    if (nextRoomId < 0) {
      // degenerate route, prohibit it and try again
      if (path.length() > 1) {
        int room2 = path[path.length() - 2];
        int room1 = path[path.length() - 1];
        failed[room2] << room1;
      } else {
        break;
      }
      currentRoomId = startRoomId;
      path.clear();
      path << startRoomId;
      progress = std::numeric_limits<int>::max();
      continue;
    }
    path << nextRoomId;
    currentRoomId = nextRoomId;
  }
  if (path.size() < 2) {
    return {};
  }
  return path;
}

QList<int> MapZone::findRoute(int startRoomId, int endRoomId)
{
  if (!costToExit.contains(endRoomId)) {
    costToExit[endRoomId] = getCostsToRoom(endRoomId, blockedRooms);
  }
  return findRouteByCost(startRoomId, endRoomId, costToExit[endRoomId]);
}

QList<int> MapZone::findRoute(int startRoomId, int endRoomId) const
{
  if (costToExit.contains(endRoomId)) {
    return findRouteByCost(startRoomId, endRoomId, costToExit[endRoomId]);
  }
  QSet<int> blocked = blockedRooms;
  return findRouteByCost(startRoomId, endRoomId, getCostsToRoom(endRoomId, blocked));
}

QPair<QList<ZoneID>, int> MapZone::findZoneRoute(const ZoneID& dest, QList<ZoneID> avoid) const
{
  const auto& conns = map->zoneConnections[name];
  if (conns.contains(dest)) {
    return { { dest }, 0 };
  }
  const auto& transits = map->zoneTransits[name];
  for (const auto& through : transits.keys()) {
    const auto& dests = transits[through];
    if (dests.contains(dest)) {
      // here -> through -> dest
      return { { through, dest }, dests[dest] };
    }
  }
  avoid << name;
  int bestCost = 0;
  QList<ZoneID> bestPath;
  for (const auto& nextID : conns) {
    if (avoid.contains(nextID)) {
      continue;
    }
    const MapZone* next = map->zone(nextID);
    auto [path, cost] = next->findZoneRoute(dest, avoid);
    if (cost <= 0 || path.isEmpty()) {
      // cost == 0 would imply a direct connection
      // but that should have been caught by transits
      continue;
    }
    int thisCost = transits.value(nextID).value(path[0], -1);
    if (thisCost <= 0) {
      // the returned path isn't accessible from here
      continue;
    }
    cost += thisCost;
    if (!bestCost || cost < bestCost) {
      bestPath = path;
      bestCost = cost;
      bestPath.insert(0, nextID);
    }
  }
  if (bestPath.isEmpty()) {
    return { {}, -1 };
  }
  return { bestPath, bestCost };
}

QList<int> MapZone::findWorldRoute(MapManager* map, int startRoomId, int endRoomId, const QList<ZoneID>& avoid)
{
  const MapRoom* startRoom = map->room(startRoomId);
  const MapRoom* endRoom = map->room(endRoomId);
  if (!startRoom || !endRoom) {
    return {};
  }

  const MapZone* startZone = map->zone(startRoom->zone);
  const MapZone* endZone = map->zone(endRoom->zone);
  if (!startZone || !endZone) {
    return {};
  }

  if (startZone == endZone) {
    return startZone->findRoute(startRoomId, endRoomId);
  }

  if (map->zoneConnections.value(startRoom->zone).contains(endRoom->zone)) {
    // Direct connection
    QList<int> path;
    for (int exit : startZone->exits[endRoom->zone]) {
      QList<int> firstLeg = startZone->findRoute(startRoomId, exit);
      if (firstLeg.isEmpty()) {
        continue;
      }
      const MapRoom* exitRoom = map->room(exit);
      for (const MapExit& roomExit : exitRoom->exits) {
        if (!endZone->roomIds.contains(roomExit.dest)) {
          continue;
        }
        QList<int> secondLeg = endZone->findRoute(roomExit.dest, endRoomId);
        if (secondLeg.isEmpty()) {
          continue;
        }
        if (path.isEmpty() || firstLeg.length() + secondLeg.length() < path.length()) {
          path = firstLeg + secondLeg;
        }
      }
    }
    return path;
  }

  auto zonePath = startZone->findZoneRoute(endRoom->zone, avoid).first;
  if (zonePath.isEmpty()) {
    // routing impossible
    return {};
  }
  ZoneID from = startRoom->zone;
  QSet<int> pathStart({ startRoomId });
  QList<int> path;
  for (const ZoneID& to : zonePath) {
    const MapZone* fromZone = map->zone(from);
    const MapZone* toZone = map->zone(to);
    QList<int> step = fromZone->pickRoute(pathStart, fromZone->exits[to], toZone->roomIds);
    if (step.isEmpty()) {
      qDebug() << "XXX: pickRoute failed when findZoneRoute succeeded";
      return {};
    }
    const MapRoom* room = map->room(step.last());
    if (!room) {
      qDebug() << "XXX: pickRoute returned invalid room";
      return {};
    }
    pathStart.clear();
    for (const MapExit& exit : room->exits) {
      if (toZone->roomIds.contains(exit.dest)) {
        pathStart << exit.dest;
      }
    }
    if (pathStart.isEmpty()) {
      qDebug() << "XXX: pickRoute returned room with invalid exits";
      return {};
    }
    path += step;
    from = to;
  }

  if (!path.isEmpty()) {
    auto step = endZone->pickRoute(pathStart, { endRoomId }, {});
    path += step;
  }

  if (!path.endsWith(endRoomId)) {
    qDebug() << "XXX: Routing failed";
    return {};
  }

  return path;
}

QList<int> MapZone::pickRoute(const QSet<int>& startRoomIds, const QSet<int>& endRoomIds, const QSet<int>& nextRoomIds) const
{
  QList<int> bestPath;
  int bestCost = 0;
  for (int startRoomId : startRoomIds) {
    for (int endRoomId : endRoomIds) {
      QList<int> path = findRoute(startRoomId, endRoomId);
      if (!path.size()) {
        continue;
      }
      if (!nextRoomIds.isEmpty()) {
        int lastRoomId = path.last();
        const MapRoom* lastRoom = map->room(lastRoomId);
        bool ok = false;
        for (const MapExit& exit : lastRoom->exits) {
          if (nextRoomIds.contains(exit.dest)) {
            ok = true;
            break;
          }
        }
        if (!ok) {
          continue;
        }
      }
      if (!bestCost || path.size() < bestCost) {
        bestPath = path;
        bestCost = path.size();
      }
    }
  }
  return bestPath;
}
