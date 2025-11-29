#include "mapsearch.h"
#include "mapmanager.h"
#include "mapzone.h"
#include "algorithms.h"
#include <QTimer>
#include <QtDebug>

using Clique = MapSearch::Clique;

QDebug operator<<(QDebug debug, const MapSearch::Grid& grid)
{
  return debug << qPrintable(grid.debug());
}

QDebug operator<<(QDebug debug, const MapSearch::Clique& clique)
{
  return debug << clique.zone;
}

QDebug operator<<(QDebug debug, const Clique::Ref& clique)
{
  if (!clique) {
    return debug << "[null clique]";
  }
  return debug << clique->zone;
}

QDebug operator<<(QDebug debug, const MapZone* zone)
{
  return debug << (zone ? zone->name : "[null zone]");
}

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
  deadEnds.clear();
  incremental.clear();
  dirtyZones.clear();
  dirtyZones << nullptr;
}

void MapSearch::markDirty(const MapZone* zone)
{
  dirtyZones << zone;
}

bool MapSearch::precompute(bool force)
{
  if (!force && dirtyZones.isEmpty()) {
    return false;
  }
  force = force || dirtyZones.contains(nullptr);
  incremental.clear();
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
    findGrids(&clique);

    QSet<int> distinctExits;
    for (const CliqueExit& exit : clique.exits) {
      distinctExits << exit.fromRoomId;
    }
    int n = distinctExits.size();
    if ((n * (n - 1)) != clique.routes.size()) {
      for (const CliqueExit& exit : clique.exits) {
        incremental.emplace_back(&clique, exit.fromRoomId);
      }
    }
    if (distinctExits.size() < 2) {
      deadEnds << &clique;
    }
  }

  dirtyZones.clear();
  QTimer::singleShot(1, this, SLOT(precomputeIncremental()));
  return true;
}

void MapSearch::precomputeRoutes()
{
  incremental.clear();
  for (Clique& clique : cliqueStore) {
    QSet<int> distinctExits;
    for (const CliqueExit& exit : clique.exits) {
      distinctExits << exit.fromRoomId;
    }
    int n = distinctExits.size();
    if ((n * (n - 1)) != clique.routes.size()) {
      for (const CliqueExit& exit : clique.exits) {
        fillRoutes(&clique, exit.fromRoomId);
      }
    }
    if (distinctExits.size() < 2) {
      deadEnds << &clique;
    }
  }
}

void MapSearch::precomputeIncremental()
{
  if (incremental.empty()) {
    return;
  }
  auto [clique, fromRoomId] = incremental.front();
  incremental.pop_front();
  fillRoutes(clique, fromRoomId);
  QTimer::singleShot(1, this, SLOT(precomputeIncremental()));
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

namespace {
using Column = QList<const MapRoom*>;

// A room can be part of a grid if:
// - It has all four cardinal exits and no others
// - All four exits are two-way
// - All four exits are contained within the same zone
// A grid can be entered and exited from any point and use Manhattan distance
struct GridCollector
{
  GridCollector(MapManager* map, int startRoomId)
  : map(map)
  {
    startRoom = map->room(startRoomId);
    if (!checkGrid(startRoom)) {
      startRoom = nullptr;
    }
  }

  bool checkGrid(const MapRoom* room)
  {
    if (!room) {
      return false;
    }
    int goodExits = 0;
    for (auto [dir, exit] : cpairs(room->exits)) {
      if (dir != "N" && dir != "E" && dir != "S" && dir != "W") {
        return false;
      }
      const MapRoom* dest = map->room(exit.dest);
      if (!dest || dest->zone != room->zone || dest->exits.value(MapRoom::reverseDir(dir)).dest != room->id) {
        return false;
      }
      goodExits += 1;
    }
    return goodExits == 4;
  }

  const MapRoom* gridStep(const MapRoom* room, const QString& dir)
  {
    if (!room) {
      return nullptr;
    }
    int nextId = room->exits.value(dir).dest;
    if (nextId <= 0) {
      return nullptr;
    }
    const MapRoom* next = map->room(nextId);
    if (!checkGrid(next)) {
      return nullptr;
    }
    return next;
  }

  Column collectGridColumn(const MapRoom* start)
  {
    Column rooms{ start };
    const MapRoom* step = start;
    while ((step = gridStep(step, "S"))) {
      rooms << step;
    }
    return rooms;
  }

  MapSearch::Grid collectGrid()
  {
    if (!startRoom) {
      return {};
    }
    Column steps{ startRoom };
    const MapRoom* step = startRoom;
    while ((step = gridStep(step, "S"))) {
      steps << step;
    }

    QList<Column> columns;
    int bestSize = 0;
    QList<Column> bestState;
    while (steps.size() > 1) {
      columns << steps;
      int size = columns.size() * steps.size();
      if (size > bestSize) {
        bestState = columns;
        bestSize = size;
      }
      for (int y = 0; y < steps.size(); y++) {
        steps[y] = gridStep(steps[y], "E");
        if (!steps[y]) {
          steps = steps.mid(0, y);
        }
      }
    }
    if (bestState.size() < 2) {
      return {};
    }
    MapSearch::Grid grid;
    grid.map = map;
    grid.grid.resize(bestState[0].size());
    for (const Column& column : bestState) {
      for (auto [y, room] : enumerate(column)) {
        grid.grid[y] << room->id;
        grid.rooms << room->id;
      }
    }
    return grid;
  }

  MapManager* map;
  const MapRoom* startRoom;
};
}

QString MapSearch::Grid::debug(const QSet<int>& exclude) const
{
  QString result = "[[[\n";
  for (int y = 0; y < grid.size(); y++) {
    QStringList row;
    for (int x = 0; x < grid[y].size(); x++) {
      int roomId = grid[y][x];
      QString label = QString::number(roomId) + "/" + QString::number(map->roomCost(roomId));
      if (roomId < 0 || exclude.contains(roomId)) {
        row << QString(label.length(), '-');
      } else {
        row << label;
      }
    }
    result += row.join("\t") + "\n";
  }
  result += "]]]";
  return result;
}

bool MapSearch::Grid::operator<(const Grid& other) const
{
  if (contains(other)) {
    return true;
  }
  if (other.contains(*this)) {
    return false;
  }
  return size() > other.size();
}

MapSearch::Grid::operator bool() const
{
  return grid.size() > 1 && grid.first().size() > 1;
}

QPair<int, int> MapSearch::Grid::pos(int sought) const
{
  for (auto [y, row] : enumerate(grid)) {
    for (auto [x, roomId] : enumerate(row)) {
      if (roomId == sought) {
        return { x, y };
      }
    }
  }
  return { -1, -1 };
}

int MapSearch::Grid::at(int x, int y) const
{
  if (x < 0 || y < 0 || y >= grid.size()) {
    return -1;
  }
  const QVector<int>& row = grid[y];
  if (x >= row.size()) {
    return -1;
  }
  return row[x];
}

bool MapSearch::Grid::mask(const QSet<int>& exclude)
{
  if (exclude.isEmpty()) {
    return true;
  }
  bool fail = false;
  int offsetLeft = -1;
  QVector<QVector<int>> masked;
  for (QVector<int>& row : grid) {
    int skipped = 0;
    bool first = true;
    QVector<int> result;
    for (int i = 0; i < row.size(); i++) {
      if (exclude.contains(row[i])) {
        row[i] = -1;
        ++skipped;
        continue;
      }
      if (first) {
        if (offsetLeft >= 0 && i != offsetLeft) {
          fail = true;
        }
        offsetLeft = i;
        first = false;
      }
      result << row[i];
    }
    if (result.isEmpty()) {
      if (masked.isEmpty()) {
        continue;
      }
    } else {
      if (result.size() < 2) {
        fail = true;
      }
      if (!masked.isEmpty() && masked.last().size() != result.size()) {
        fail = true;
      }
    }
    masked << result;
  }
  while (!masked.isEmpty() && masked.last().isEmpty()) {
    masked.removeLast();
  }
  if (masked.size() < 2) {
    fail = true;
  }
  if (fail) {
    return false;
  }
  grid = masked;
  rooms.clear();
  for (const QVector<int>& row : grid) {
    for (int roomId : row) {
      rooms << roomId;
    }
  }
  return true;
}

void MapSearch::findGrids(Clique::MRefR clique)
{
  if (!clique || !clique->grids.isEmpty()) {
    return;
  }
  QList<Grid> grids;
  QSet<int> visited;
  for (int roomId : clique->roomIds) {
    if (visited.contains(roomId)) {
      continue;
    }
    visited << roomId;
    GridCollector collector(map, roomId);
    if (collector.startRoom) {
      Grid grid = collector.collectGrid();
      visited.unite(grid.rooms);
      if (grid.rooms.size() > 4) {
        bool found = false;
        for (int i = 0; i < grids.size(); i++) {
          Grid& other = grids[i];
          if (other.contains(grid)) {
            found = true;
            break;
          } else if (grid.contains(other)) {
            found = true;
            grids[i] = grid;
            break;
          }
        }
        if (!found) {
          grids << grid;
        }
      }
    }
  }
  if (grids.isEmpty()) {
    return;
  }
  std::sort(grids.begin(), grids.end());
  QSet<int> exclude;
  for (int i = 0; i < grids.size(); i++) {
    if (grids[i].mask(exclude)) {
      exclude.unite(grids[i].rooms);
    } else {
      grids.removeAt(i);
      --i;
    }
  }
  clique->grids = grids;
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

MapSearch::Route MapSearch::findRouteInClique(Clique::RefR clique, int startRoomId, int endRoomId, const QMap<int, int>& costs, int maxCost) const
{
  if (startRoomId == endRoomId) {
    return { { endRoomId }, 0 };
  }
  if (!costs.contains(startRoomId)) {
    // can't get there from here
    return { {}, 0 };
  }
  if (maxCost <= 0) {
    maxCost = costs[endRoomId];
  }
  const MapRoom* room = map->room(endRoomId);
  if (room->exitRooms().contains(startRoomId)) {
    return { { startRoomId, endRoomId }, map->roomCost(endRoomId) };
  }
  bool gridStepped = false;
  QList<int> gridRoute{ endRoomId };
  int gridCost = 0;
  for (const Grid& grid : clique->grids) {
    int thisGridCost = 0;
    do {
      gridStepped = false;
      QPair<int, int> pos = grid.pos(endRoomId);
      if (pos.first < 0) {
        break;
      }
      int bestEntrance = -1;
      int bestCost = maxCost;
      bool bestIsExit = false;
      static QPair<int, int> offsets[] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
      for (QPair<int, int> offset : offsets) {
        int nextRoomId = grid.at(pos.first + offset.first, pos.second + offset.second);
        int cost = costs.value(nextRoomId, maxCost);
        if (nextRoomId == startRoomId) {
          thisGridCost += cost;
          gridRoute.insert(0, nextRoomId);
          return { gridRoute, thisGridCost };
        }
        if (bestIsExit ? cost < bestCost : cost <= bestCost) {
          bestCost = cost;
          bestEntrance = nextRoomId;
          bestIsExit = !grid.contains(nextRoomId);
          thisGridCost += map->roomCost(nextRoomId);
        }
      }
      if (bestEntrance >= 0) {
        gridStepped = true;
        gridRoute.insert(0, bestEntrance);
        endRoomId = bestEntrance;
        maxCost = bestCost;
      }
    } while (gridStepped);
    if (gridRoute.size() > 1) {
      gridCost = thisGridCost;
      break;
    }
  }
  if (gridRoute.size() > 1) {
    Route enterRoute = findRouteInClique(clique, startRoomId, endRoomId, costs, costs[endRoomId]);
    if (enterRoute.rooms.isEmpty()) {
      return {};
    } else {
      return { enterRoute.rooms + gridRoute.mid(1), gridCost + enterRoute.cost };
    }
  }
  Route bestRoute;
  for (int nextRoomId : room->entrances) {
    if (nextRoomId == startRoomId || costs.value(nextRoomId, maxCost) >= maxCost) {
      continue;
    }
    Route route = findRouteInClique(clique, startRoomId, nextRoomId, costs, costs[nextRoomId]);
    if (route.rooms.isEmpty()) {
      continue;
    }
    if (bestRoute.rooms.isEmpty() || bestRoute.cost > route.cost) {
      bestRoute = route;
    }
  }
  if (bestRoute.rooms.isEmpty()) {
    return { {}, 0 };
  }
  bestRoute.rooms << endRoomId;
  bestRoute.cost += map->roomCost(endRoomId);
  return bestRoute;
}

void MapSearch::fillRoutes(Clique::MRefR clique, int startRoomId)
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
    clique->routes[key] = findRouteInClique(clique, startRoomId, endRoomId, costs);
  }
}

QList<Clique::Ref> MapSearch::findCliqueRoute(int startRoomId, int endRoomId, const QStringList& avoidZones) const
{
  Clique::Ref fromClique = findClique(startRoomId);
  Clique::Ref toClique = findClique(endRoomId);
  CliqueRoute route = findCliqueRoute(fromClique, toClique, collectCliques(avoidZones));
  return route.cliques;
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

MapSearch::CliqueRoute MapSearch::findCliqueRoute(Clique::RefR fromClique, Clique::RefR toClique, QList<Clique::Ref> avoid) const
{
  // Step 1: direct connection
  for (const CliqueExit& exit : fromClique->exits) {
    if (exit.toClique == toClique) {
      return { { toClique }, 0 };
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
      return { { &through, toClique }, through.routes[qMakePair(foundFrom, foundTo)].cost };
    }
  }
  // Step 3: recursive search
  avoid << fromClique;
  CliqueRoute bestPath;
  for (const CliqueExit& exit : fromClique->exits) {
    Clique::Ref next = exit.toClique;
    if (avoid.contains(next) && next != toClique) {
      continue;
    }
    if (next != toClique && deadEnds.contains(next)) {
      continue;
    }
    CliqueRoute path = findCliqueRoute(next, toClique, avoid);
    if (path.cliques.isEmpty()) {
      continue;
    }
    Clique::Ref transitTo = path.cliques[0];
    int transitCost = -1;
    for (const CliqueExit& nextExit : next->exits) {
      if (nextExit.toClique == transitTo) {
        QPair<int, int> key(exit.toRoomId, nextExit.fromRoomId);
        Route thisTransit = next->routes.value(key);
        if (!thisTransit.rooms.isEmpty() && (transitCost < 0 || thisTransit.cost < transitCost)) {
          transitCost = thisTransit.cost;
        }
      }
    }
    if (!transitCost) {
      continue;
    }
    int cost = path.cost + transitCost;
    if (bestPath.cliques.isEmpty() || cost < bestPath.cost) {
      bestPath.cliques = path.cliques;
      bestPath.cost = cost;
      bestPath.cliques.insert(0, next);
    }
  }
  if (bestPath.cliques.isEmpty()) {
    return { {}, -1 };
  }
  return bestPath;
}

QList<int> MapSearch::findRoute(int startRoomId, int endRoomId, const QStringList& avoidZones) const
{
  Clique::Ref startClique = findClique(startRoomId);
  Clique::Ref endClique = findClique(endRoomId);
  if (!startClique || !endClique) {
    return {};
  }

  if (startClique == endClique) {
    QMap<int, int> costs = getCosts(startClique, startRoomId, endRoomId);
    return findRouteInClique(startClique, startRoomId, endRoomId, costs).rooms;
  }

  QList<Clique::Ref> avoidCliques = collectCliques(avoidZones);
  QList<Clique::Ref> cliqueRoute = findCliqueRoute(startClique, endClique, avoidCliques).cliques;
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
      if (exit.toClique == cliqueRoute[i + 1]) {
        outRooms << exit.fromRoomId;
        nextRooms << exit.toRoomId;
      }
    }
    steps << CliqueStep{ clique, outRooms, nextRooms };
  }
  steps << CliqueStep{ cliqueRoute.last(), { endRoomId }, {} };

  return findRoute(steps, 0, startRoomId).rooms;
}

QList<int> MapSearch::findRoute(int startRoomId, const QString& destZone, const QStringList& avoidZones) const
{
  Clique::Ref startClique = findClique(startRoomId);
  QList<Clique::Ref> destCliques;
  {
    QList<Clique::MRef> nonConstDestCliques = cliques.value(destZone);
    destCliques = QList<Clique::Ref>(nonConstDestCliques.begin(), nonConstDestCliques.end());
  }
  if (!startClique || destCliques.isEmpty() || destCliques.contains(startClique)) {
    return {};
  }

  QList<Clique::Ref> avoidCliques = collectCliques(avoidZones);
  CliqueRoute cliqueRoute;
  for (Clique::Ref endClique : destCliques) {
    CliqueRoute testRoute = findCliqueRoute(startClique, endClique, avoidCliques);
    if (testRoute.cliques.isEmpty()) {
      continue;
    }
    Q_ASSERT(testRoute.cliques.last() == endClique);
    if (cliqueRoute.cliques.isEmpty() || testRoute.cost < cliqueRoute.cost) {
      cliqueRoute = testRoute;
    }
  }
  cliqueRoute.cliques.insert(0, startClique);

  QList<CliqueStep> steps;
  int maxRoute = cliqueRoute.cliques.size() - 1;
  for (auto [ i, clique ] : enumerate(cliqueRoute.cliques)) {
    if (i == maxRoute) {
      break;
    }
    QSet<int> outRooms, nextRooms;
    for (const CliqueExit& exit : clique->exits) {
      if (exit.toClique == cliqueRoute.cliques[i + 1]) {
        outRooms << exit.fromRoomId;
        nextRooms << exit.toRoomId;
      }
    }
    steps << CliqueStep{ clique, outRooms, nextRooms };
  }
  steps << CliqueStep{ cliqueRoute.cliques.last(), steps.last().nextRoomIds, {} };

  return findRoute(steps, 0, startRoomId).rooms;
}

MapSearch::Route MapSearch::findRoute(const QList<MapSearch::CliqueStep>& steps, int index, int startRoomId) const
{
  const auto& step = steps[index];
  QMap<int, int> costs;
  bool lastStep = (index == steps.length() - 1);
  if (index == 0) {
    costs = getCosts(step.clique, startRoomId);
  } else if (lastStep && !step.endRoomIds.isEmpty()) {
    costs = getCosts(step.clique, startRoomId, *step.endRoomIds.begin());
  }
  Route bestRoute, stepRoute;
  for (int endRoomId : step.endRoomIds) {
    if (index == 0 || lastStep) {
      stepRoute = findRouteInClique(step.clique, startRoomId, endRoomId, costs);
    } else {
      stepRoute = step.clique->routes[qMakePair(startRoomId, endRoomId)];
    }
    if (stepRoute.rooms.isEmpty()) {
      continue;
    }
    if (lastStep) {
      if (bestRoute.rooms.isEmpty() || stepRoute.cost < bestRoute.cost) {
        bestRoute = stepRoute;
      }
      continue;
    }
    const MapRoom* endRoom = map->room(endRoomId);
    Route bestNext;
    for (const MapExit& exit : endRoom->exits) {
      if (step.nextRoomIds.contains(exit.dest)) {
        Route nextRoute = findRoute(steps, index + 1, exit.dest);
        if (nextRoute.rooms.isEmpty()) {
          continue;
        }
        if (bestNext.rooms.isEmpty() || bestNext.cost < nextRoute.cost) {
          bestNext = nextRoute;
        }
      }
    }
    if (bestNext.rooms.isEmpty()) {
      continue;
    }
    if (bestRoute.rooms.isEmpty() || stepRoute.cost + bestNext.cost < bestRoute.cost) {
      bestRoute.rooms = stepRoute.rooms + bestNext.rooms;
      bestRoute.cost = stepRoute.cost + bestNext.cost;
    }
  }
  return bestRoute;
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
