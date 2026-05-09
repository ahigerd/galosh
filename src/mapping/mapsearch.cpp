#include "mapsearch.h"
#include "mapmanager.h"
#include "mapzone.h"
#include "algorithms.h"
#include <QTimer>
#include <QtDebug>

using Clique = MapSearch::Clique;

MapSearch::MapSearch(MapManager* map)
: map(map)
{
  dirtyZones << nullptr;
}

void MapSearch::reset()
{
  nodes.clear();
  cliques.clear();
  cliqueStore.clear();
  pendingRoomIds.clear();
  dirtyZones.clear();
  dirtyZones << nullptr;
}

void MapSearch::markDirty(const MapZone* zone)
{
  dirtyZones << zone;
}

bool MapSearch::precompute(bool force)
{
  force = force || nodes.isEmpty() || dirtyZones.contains(nullptr);
  if (!force && dirtyZones.isEmpty()) {
    return false;
  }

  if (force) {
    reset();
  } else {
    for (const MapZone* zone : dirtyZones) {
      auto zoneCliques = cliques.take(zone->name);
      for (Clique::MRef clique : zoneCliques) {
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
  }

  nodes.clear();
  for (auto [roomId, room] : cpairs(map->rooms)) {
    if (!nodes.contains(roomId)) {
      generateNodes(&room);
    }
  }

  for (auto [nodeId, node] : cpairs(nodes)) {
    for (int destId : node.exits) {
      Node& dest = nodes[destId];
      if (!dest.entrances.contains(nodeId)) {
        dest.entrances << nodeId;
      }
    }
  }

  dirtyZones.clear();
  return true;
}

Clique::MRef MapSearch::newClique(const MapZone* parent)
{
  cliqueStore.push_back(Clique());
  Clique::MRef clique = &cliqueStore.back();
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
    Clique::MRef clique = newClique(zone);
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
    Clique::MRef clique = newClique(zone);
    clique->roomIds << roomId;
    crawlClique(clique, roomId);

    // Check for clique intersections and merge if needed
    for (Clique::MRef other : QList<Clique::MRef>(cliques[zone->name])) {
      if (clique == other || !clique->roomIds.intersects(other->roomIds)) {
        continue;
      }
      other->roomIds.unite(clique->roomIds);
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

QList<Clique::Ref> MapSearch::cliquesForZone(const MapZone* zone) const
{
  QList<Clique::Ref> result;
  for (Clique::Ref clique : cliques.value(zone->name)) {
    result << clique;
  }
  return result;
}

void MapSearch::crawlClique(Clique::MRefR clique, int roomId)
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
        crawlClique(clique, exit.dest);
      }
    }
  }
}

void MapSearch::resolveExits(Clique::MRefR clique)
{
  for (auto [zoneId, exitRoomIds] : cpairs(clique->zone->exits)) {
    const MapZone* destZone = map->zone(zoneId);
    if (!destZone) {
      continue;
    }
    for (int roomId : exitRoomIds) {
      if (!clique->roomIds.contains(roomId)) {
        // exit belongs to a different clique
        continue;
      }
      const MapRoom* room = map->room(roomId);
      if (!room) {
        continue;
      }
      for (const MapExit& exit : room->exits) {
        const MapRoom* dest = map->room(exit.dest);
        if (dest && dest->zone == zoneId) {
          Clique::MRef toClique = findClique(zoneId, exit.dest);
          if (!toClique) {
            qDebug() << "XXX: broken clique, debug this";
            continue;
          }
          clique->exits << (CliqueExit){ roomId, toClique, exit.dest };
        }
      }
    }
  }
}

Clique::MRef MapSearch::findClique(const QString& zoneName, int roomId) const
{
  for (Clique::MRef clique : cliques.value(zoneName)) {
    if (clique->roomIds.contains(roomId)) {
      return clique;
    }
  }
  return nullptr;
}

Clique::MRef MapSearch::findClique(int roomId) const
{
  const MapRoom* room = map->room(roomId);
  if (!room) {
    return nullptr;
  }
  return findClique(room->zone, roomId);
}

QMap<int, int> MapSearch::getCosts(Clique::RefR clique, int startRoomId, int endRoomId) const
{
  QMap<int, int> costs;
  costs[startRoomId] = map->roomCost(startRoomId);

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
  int round = costs.value(startRoomId);
  while (!frontier.isEmpty()) {
    for (const MapRoom* room : frontier) {
      if (!room) {
        continue;
      }
      if (costs.value(room->id) != round) {
        nextFrontier << room;
        continue;
      }
      for (const MapExit& exit : room->exits) {
        if (costs.contains(exit.dest) || !clique->roomIds.contains(exit.dest)) {
          continue;
        }
        costs[exit.dest] = round + map->roomCost(exit.dest);
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

QList<Clique::Ref> MapSearch::collectCliques(const QStringList& zones) const
{
  QList<Clique::Ref> result;
  for (const QString& zone : zones) {
    for (Clique::RefR clique : cliques.value(zone)) {
      result << clique;
    }
  }
  return result;
}

void MapSearch::generateNodes(const MapRoom* start)
{
  Node& node = nodes[start->id];
  if (node.roomId < 0) {
    node.roomId = start->id;
    node.cost = map->roomCosts.value(start->roomType);
    if (!node.cost) {
      node.cost = 1;
    }
  }

  for (auto [dir, exit] : cpairs(start->exits)) {
    int destId = exit.dest;
    if (destId < 0) {
      continue;
    }
    const MapRoom* dest = map->room(destId);
    if (!dest) {
      continue;
    }
    if (!node.exits.contains(destId)) {
      node.exits << destId;
    }
    if (!nodes.contains(destId)) {
      generateNodes(dest);
    }
  }
}

struct StackCounter
{
  StackCounter(int* depth) : depth(depth), indent(*depth, ' ') {
    indent += indent;
    ++(*depth);
  }
  ~StackCounter() { --(*depth); }
  operator const char*() const { return indent.constData(); }
  int* depth;
  QByteArray indent;
};

QHash<int, int> MapSearch::costsFromNode(int startRoomId, bool reverse, const QSet<int>& avoidRooms) const
{
  QHash<int, int> costs;
  costs[startRoomId] = 1;
  QSet<int> frontier({ startRoomId });
  while (!frontier.isEmpty()) {
    QSet<int> newFrontier;
    for (int roomId : frontier) {
      int baseCost = costs[roomId];
      const Node& node = nodes.value(roomId);
      for (int destId : (reverse ? node.entrances : node.exits)) {
        if (avoidRooms.contains(destId)) {
          continue;
        }
        const Node& dest = nodes.value(destId);
        int newCost = baseCost + dest.cost;
        int oldCost = costs.value(destId, -1);
        if (oldCost < 0 || newCost < oldCost) {
          costs[destId] = newCost;
          newFrontier << destId;
        }
      }
    }
    frontier = newFrontier;
  }
  return costs;
}

QPair<QList<int>, int> MapSearch::findRoute(int startRoomId, int endRoomId, const QSet<int>& avoidRooms) const
{
  QHash<int, int> costs = costsFromNode(endRoomId, true, avoidRooms);
  if (!costs.contains(startRoomId)) {
    return {};
  }

  int traverse = startRoomId;
  QList<int> route({ startRoomId });
  while (traverse != endRoomId) {
    const auto& node = nodes[traverse];
    int bestExit = -1;
    int bestCost = -1;
    for (int exit : node.exits) {
      if (route.contains(exit)) {
        // this would be backtracking or sidetracking
        continue;
      }
      int cost = costs[exit];
      if (bestCost < 0 || cost < bestCost) {
        bestExit = exit;
        bestCost = cost;
      }
    }
    if (bestExit < 0) {
      qDebug() << "XXX: no route";
      return {};
    }
    route << bestExit;
    traverse = bestExit;
  }
  return qMakePair(route, costs[startRoomId]);
}

QList<int> MapSearch::findRoute(int startRoomId, int endRoomId, const QStringList& avoidZones) const
{
  QSet<int> avoidRooms;
  for (const QString& zoneName : avoidZones) {
    const MapZone* zone = map->zone(zoneName);
    if (zone && !zone->roomIds.contains(startRoomId) && !zone->roomIds.contains(endRoomId)) {
      avoidRooms += zone->roomIds;
    }
  }

  return findRoute(startRoomId, endRoomId, avoidRooms).first;
}

QList<int> MapSearch::findRoute(int startRoomId, const QString& destZone, const QStringList& avoidZones) const
{
  const MapZone* zone = map->zone(destZone);
  if (!zone) {
    return {};
  }

  QSet<int> avoidRooms;
  for (const QString& zoneName : avoidZones) {
    if (zoneName == destZone) {
      continue;
    }
    const MapZone* zone = map->zone(zoneName);
    if (zone && !zone->roomIds.contains(startRoomId)) {
      avoidRooms += zone->roomIds;
    }
  }

  QHash<int, int> forwardCosts = costsFromNode(startRoomId, false, avoidRooms);
  int endRoomId = -1;
  int bestCost = -1;
  for (const QSet<int>& exits : zone->exits) {
    for (int roomId : exits) {
      int cost = forwardCosts[roomId];
      if (bestCost < 0 || cost < bestCost) {
        endRoomId = roomId;
        bestCost = cost;
      }
    }
  }

  return findRoute(startRoomId, endRoomId, avoidRooms).first;
}

QStringList MapSearch::routeDirections(const QList<int>& route) const
{
  if (route.length() < 2) {
    return {};
  }
  QStringList path;
  int startRoomId = route.first();
  const MapRoom* room = map->room(startRoomId);
  for (int step : route) {
    if (step == startRoomId) {
      continue;
    }
    if (!room) {
      return {};
    }
    QString dir = room->findExit(step);
    if (dir.isEmpty()) {
      return {};
    }
    path << dir;
    room = map->room(step);
  }
  return path;
}
