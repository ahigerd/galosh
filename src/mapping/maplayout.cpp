#include "maplayout.h"
#include "mapmanager.h"
#include "mapzone.h"
#include "algorithms.h"
#include <QTabWidget>
#include <QPainter>
#include <QPainterPath>
#include <QImage>
#include <QPixmap>
#include <QLabel>
#include <QScrollArea>
#include <QtDebug>
#include <cmath>

static const QStringList dirOrder{ "N", "E", "S", "W", "U", "D", "NE", "SE", "SW", "NW" };

static const QMap<QString, QPointF> dirVectors = {
  { "N", QPointF(0, -1) },
  { "E", QPointF(1, 0) },
  { "S", QPointF(0, 1) },
  { "W", QPointF(-1, 0) },
  { "NE", QPointF(1, -1) },
  { "SE", QPointF(1, 1) },
  { "SW", QPointF(-1, 1) },
  { "NW", QPointF(-1, -1) },
  { "U", QPointF(-1, -1) },
  { "D", QPointF(1, 1) },
};

static const QMap<QString, QPointF> oneWayOffset = {
  { "N", QPointF(0.1, 0) },
  { "E", QPointF(0, 0.1) },
  { "S", QPointF(-0.1, 0) },
  { "W", QPointF(0, -0.1) },
};

static constexpr int ROOM_SIZE = 5;
static constexpr double EXIT_OFFSET = ROOM_SIZE / 2.0;
static constexpr int STEP_SIZE = 10;
static constexpr int COORD_SCALE = ROOM_SIZE + STEP_SIZE;
static constexpr double RENDER_SCALE = 5;
static const QPointF ROOM_OFFSET(ROOM_SIZE / 2.0, ROOM_SIZE / 2.0);
static const QSizeF ROOM_SIZEF(ROOM_SIZE, ROOM_SIZE);

QList<QColor> initNonlinearColors()
{
  QList<QColor> colors;
  for (int i = 0; i < 17; i++) {
    QColor c = QColor::fromHsl((i * 1080 / 17) % 360, 255, 160);
    while (qGray(c.rgb()) < 140) {
      c = c.lighter();
    }
    colors << c;
  }
  return colors;
}
static const QList<QColor> nlColors = initNonlinearColors();

static QPair<int, int> pointToPair(const QPointF& pt)
{
  return qMakePair(int(pt.x() * 1024 + 0.5), int(pt.y() * 1024 + 0.5));
}

static inline double magSqr(const QPointF& pt)
{
  return pt.x() * pt.x() + pt.y() * pt.y();
}

static QPointF normalized(const QPointF& pt)
{
  if (pt.isNull()) {
    return pt;
  }
  return pt / std::sqrt(magSqr(pt));
}

static QPointF toEdge(const QPointF& pt)
{
  double x = pt.x();
  double y = pt.y();
  if (x < 0) {
    y /= -x;
    x = -1;
  } else if (x > 0) {
    y /= x;
    x = 1;
  } else {
    y = y < 0 ? -1 : 1;
  }
  if (y < -1) {
    x /= -y;
    y = -1;
  } else if (y > 1) {
    x /= y;
    y = 1;
  }
  return QPointF(x, y);
}

MapLayout::MapLayout(MapManager* map)
: map(map), search(nullptr)
{
  // initializers only
}

void MapLayout::loadZone(const MapZone* zone)
{
  if (zone) {
    search = map->search();
    if (!search->precompute()) {
      // If no changes occurred in the search computation,
      // it's safe to short-circuit and keep what we have,
      // assuming we aren't moving to a new zone.
      if (currentZone == zone->name) {
        return;
      }
    }
  } else {
    currentZone.clear();
  }

  coords.clear();
  roomLayers.clear();
  coordsRev.clear();
  pathPoints.clear();
  oneWayExits.clear();
  layers.clear();
  pendingLayers.clear();
  colors.clear();
  boundingBox = QRectF();

  if (!zone) {
    return;
  }

  currentZone = zone->name;

  QMap<QPair<int, QString>, int> zoneExits;
  oneWayExits.clear();
  for (const MapSearch::Clique* clique : search->cliquesForZone(zone)) {
    for (int roomId : clique->roomIds) {
      const MapRoom* room = map->room(roomId);
      if (!room) {
        continue;
      }
      for (auto [dir, exit] : cpairs(room->exits)) {
        const MapRoom* dest = map->room(exit.dest);
        if (!clique->roomIds.contains(exit.dest)) {
          if (dest && dest->zone != zone->name) {
            zoneExits[qMakePair(roomId, dir)] = exit.dest;
          }
          continue;
        }
        if (dest && !dest->hasExitTo(roomId)) {
          oneWayExits[exit.dest] << roomId;
        }
      }
    }

    coords.clear();
    coordsRev.clear();
    boundingBox = QRectF();
    int roomId = -1;
    for (const auto& exit : clique->exits) {
      if (roomId < 0 || exit.fromRoomId < roomId) {
        roomId = exit.fromRoomId;
      }
    }
    if (roomId < 0) {
      for (int id : clique->roomIds) {
        if (roomId < 0 || id < roomId) {
          roomId = id;
        }
      }
    }

    if (boundingBox.isEmpty()) {
      coords[roomId] = QPointF(1, 1);
    } else {
      coords[roomId] = QPointF(boundingBox.right() + 5, 1);
    }
    loadClique(clique, roomId, 0);
    relax();
    layers << (LayerData){ clique, coords, boundingBox, 0, {}, {}, {} };

    do {
      auto layersCopy = pendingLayers;
      pendingLayers.clear();
      for (auto [ startRoomId, zIndex ] : cpairs(layersCopy)) {
        if (roomLayers.contains(startRoomId)) {
          continue;
        }
        coords.clear();
        coordsRev.clear();
        boundingBox = QRectF();
        coords[startRoomId] = QPointF(1, 1);
        loadClique(clique, startRoomId, zIndex);
        relax();
        layers << (LayerData){ clique, coords, boundingBox, zIndex, {}, {}, {} };
      }
    } while (!pendingLayers.isEmpty());
  }

  coords.clear();
  coordsRev.clear();
  boundingBox = QRectF();
  QPointF offset;
  for (LayerData& layer : layers) {
    for (int roomId : layer.coords.keys()) {
      QPointF pos = layer.coords[roomId] + offset;
      layer.coords[roomId] = coords[roomId] = pos;
      coordsRev[pointToPair(pos)] = roomId;
      for (const MapExit& exit : map->room(roomId)->exits) {
        if (layer.coords.contains(exit.dest)) {
          continue;
        }
        const MapRoom* dest = map->room(exit.dest);
        if (dest && dest->zone == zone->name) {
          layer.layerExits << qMakePair(roomId, exit.dest);
        }
      }
    }
    offset.ry() += layer.boundingBox.height();
    calculateRegion(layer);
  }

  for (LayerData& layer : layers) {
    for (auto [ fromId, toId ] : layer.layerExits) {
      LayerData* other = findLayer(toId);
      Q_ASSERT(other);
      other->layerExits << qMakePair(toId, fromId);
    }
  }

  QList<LayerData*> done;
  QSet<const MapSearch::Clique*> rootCliques;
  QRect bb;
  QRegion locked;
  while (done.size() < layers.size()) {
    for (LayerData& layer : layers) {
      if (done.contains(&layer)) {
        continue;
      }
      if (done.isEmpty() || !rootCliques.contains(layer.source)) {
        locked += layer.region;
        bb = locked.boundingRect();
        done << &layer;
        rootCliques << layer.source;
        continue;
      }
      QSet<QPair<int, int>> tensionRooms;
      for (LayerData* other : done) {
        for (auto [ fromId, toId ] : layer.layerExits) {
          if (other->coords.contains(toId)) {
            tensionRooms << qMakePair(fromId, toId);
          }
        }
      }
      if (tensionRooms.isEmpty()) {
        // doesn't connect yet, come back later
        qDebug() << "no connections from layer to done" << layer.layerExits;
        continue;
      }
      QRect layerBB = layer.boundingBox.toRect();
      int x0 = -(layerBB.left() - bb.left()) - layerBB.width() - 1;
      int x1 = x0 + bb.width() + layerBB.width() + 3;
      int y0 = -(layerBB.top() - bb.top()) - layerBB.height() - 1;
      int y1 = y0 + bb.height() + layerBB.height() + 3;
      int bestTension = -1;
      QPointF bestPos;
      for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
          if (layer.region.translated(x, y).intersects(locked)) {
            continue;
          }
          QPointF offset(x, y);
          QMap<int, QPointF> substitutions;
          for (auto [ fromId, _ ] : tensionRooms) {
            substitutions[fromId] = layer.coords[fromId] + offset;
          }
          int t = 0;
          for (auto [ _, toId ] : tensionRooms) {
            t += tension(toId, substitutions, true);
          }
          if (bestTension < 0 || bestTension > t || (bestTension == t && magSqr(offset) < magSqr(bestPos))) {
            bestTension = t;
            bestPos = offset;
          }
        }
      }
      if (bestTension < 0) {
        qDebug() << "no valid placement?";
        break;
      }
      for (auto [ roomId, pos ] : pairs(layer.coords)) {
        pos += bestPos;
        coords[roomId] = pos;
      }
      layer.region.translate(bestPos.toPoint());
      layer.polygon.translate(bestPos.toPoint());
      locked = locked.united(layer.region);
      bb = locked.boundingRect();
      done << &layer;
      rootCliques << layer.source;
    }
    break;
  }

  calculateBoundingBox();
  boundingBox.adjust(-1, -1, 1, 1);
  boundingBox = QRectF(boundingBox.topLeft() * COORD_SCALE, boundingBox.bottomRight() * COORD_SCALE);

  // update final coordinates
  coords.clear();
  coordsRev.clear();
  for (const LayerData& layer : layers) {
    coords.insert(layer.coords);
    for (auto [roomId, pos] : cpairs(layer.coords)) {
      coordsRev[pointToPair(pos)] = roomId;
    }
  }
  markPathPoints();

  for (auto [pair, destRoomId] : cpairs(zoneExits)) {
    auto [roomId, dir] = pair;
    coordsRev[pointToPair(coords.value(roomId) + dirVectors[dir] * 0.75)] = destRoomId;
  }

  title = zone->name;
}

void MapLayout::loadClique(const MapSearch::Clique* clique, int roomId, int zIndex)
{
  const MapRoom* room = map->room(roomId);
  Q_ASSERT(room->zone == currentZone);
  if (roomLayers.contains(roomId)) {
    if (zIndex != roomLayers[roomId]) {
      qDebug() << "revisit?" << roomId << roomLayers[roomId] << zIndex;
    }
    return;
  }
  roomLayers[roomId] = zIndex;
  for (auto [ dir, exit ] : cpairs(room->exits)) {
    if (!clique->roomIds.contains(exit.dest)) {
      continue;
    }
    QPointF pos = coords.value(roomId);
    int dest = exit.dest;
    int newZIndex = zIndex;
    if (dir == "U" || dir == "D") {
      const MapRoom* destRoom = map->room(dest);
      if (!destRoom) {
        continue;
      }
      QString rev = destRoom->findExit(roomId);
      bool isVertical = (rev == "U" || rev == "D");
      newZIndex += (dir == "U" ? 1 : -1);
      if (rev.isEmpty() || (isVertical && destRoom->exits.size() > 1)) {
        if (pendingLayers.contains(dest)) {
          if (pendingLayers.value(dest) != newZIndex) {
            qDebug() << "layer conflict" << roomId << zIndex << dir << dest << "from" << pendingLayers[dest] << "to" << newZIndex;
          }
          continue;
        }
        pendingLayers[dest] = newZIndex;
        continue;
      }
    }
    if (!clique->roomIds.contains(dest)) {
      continue;
    }
    QPointF nextPos = pos + dirVectors.value(dir, QPointF(0.1, 0.1));
    if (coords.contains(dest)) {
      continue;
    }
    auto rev = pointToPair(nextPos);
    while (coordsRev.contains(rev)) {
      nextPos = (nextPos + pos) / 2;
      rev = pointToPair(nextPos);
    }
    coords[dest] = nextPos;
    coordsRev[rev] = dest;
    loadClique(clique, dest, newZIndex);
  }
}

void MapLayout::calculateBoundingBox()
{
  boundingBox = QRectF();
  for (const QPointF& pos : coords) {
    if (boundingBox.left() > pos.x()) {
      boundingBox.setLeft(pos.x());
    } else if (boundingBox.right() < pos.x()) {
      boundingBox.setRight(pos.x());
    }
    if (boundingBox.top() > pos.y()) {
      boundingBox.setTop(pos.y());
    } else if (boundingBox.bottom() < pos.y()) {
      boundingBox.setBottom(pos.y());
    }
  }
}

double MapLayout::tension(int roomId, int destRoomId, const QString& dir, const QMap<int, QPointF>& substitutions, bool weightHigh) const
{
  if (!coords.contains(destRoomId)) {
    return -1;
  }
  const MapRoom* dest = map->room(destRoomId);
  QPointF startPoint = substitutions.value(roomId, coords[roomId]);
  QPointF endPoint = substitutions.value(destRoomId, coords[destRoomId]);
  QString reverse = dest->findExit(roomId);
  if (reverse.isEmpty()) {
    reverse = MapRoom::reverseDir(dir);
  }
  QPointF step = startPoint + dirVectors[dir] * 0.25;
  QPointF revStep = endPoint + dirVectors[reverse] * 0.25;
  double dx = step.x() - revStep.x();
  double dy = step.y() - revStep.y();
  bool diagonal = (dir == "U" || dir == "D");
  if (diagonal && dx >= -2 && dx <= 2) {
    dy -= dx;
    static constexpr double sqrt2 = std::sqrt(2);
    dx *= sqrt2;
  }
  double tension = std::sqrt((dx * dx) + (dy * dy));
  if (tension <= 1) {
    return 0;
  }
  if (step.x() != revStep.x() && step.y() != revStep.y()) {
    // nonlinearity introduces a lot of extra tension
    // but past a certain point it stops being relevant
    if (weightHigh) {
      // Slightly penalize vertical distance more than horizontal
      tension *= (dy < 0 ? 1 - dy * 0.1 : 1 + dy * 0.1);
      double slope = dy ? qAbs(dy / dx) : 0;
      if (slope >= 0.9 && slope <= 1.1 && (dx <= -4 || dx >= 4)) {
        // long nearly-diagonal lines are very bad because they're highly likely to intersect rooms
        tension *= 10;
      }
    } else if (tension > 15) {
      tension *= 10;
    } else if (tension > 3) {
      tension = 60 + 2 * std::sqrt(tension - 3);
    } else {
      tension *= 20.0;
    }
    tension += 5;
  }
  // if it's supposed to be a straight connection, penalize going the wrong way
  if (reverse == MapRoom::reverseDir(dir)) {
    if (signum(int(endPoint.x() - startPoint.x())) != int(dirVectors[dir].x())) {
      tension *= 20 * qAbs(dx);
    }
    if (signum(int(endPoint.y() - startPoint.y())) != int(dirVectors[dir].y())) {
      tension *= 20 * qAbs(dy);
    }
  }
  return tension;
}

double MapLayout::tension(int roomId, const QMap<int, QPointF>& substitutions, bool weightHigh) const
{
  double total = 0;
  const MapRoom* room = map->room(roomId);
  for (auto [ dir, exit ] : cpairs(room->exits)) {
    int t = tension(roomId, exit.dest, dir, substitutions, weightHigh);
    if (t > 0) {
      total += t;
    }
  }
  for (int sourceRoomId : oneWayExits.value(roomId)) {
    const MapRoom* source = map->room(sourceRoomId);
    if (!source) {
      continue;
    }
    int t = tension(roomId, sourceRoomId, MapRoom::reverseDir(source->findExit(roomId)), substitutions, weightHigh);
    if (t > 0) {
      total += t * 0.5;
    }
  }
  return total;
}

double MapLayout::tension(const QMap<int, QPointF>& substitutions) const
{
  double total = 0;
  for (int roomId : keys(coords)) {
    total += tension(roomId, substitutions);
  }
  return total;
}

void MapLayout::relattice()
{
  // Find X and Y values that are in use
  QSet<double> xValues, yValues;
  for (const QPointF& pt : coords) {
    xValues << pt.x();
    yValues << pt.y();
  }
  // Sort the coordinates
  int i = 1;
  QList<double> xSort(xValues.begin(), xValues.end()), ySort(yValues.begin(), yValues.end());
  std::sort(xSort.begin(), xSort.end());
  std::sort(ySort.begin(), ySort.end());
  // Create a translation map from the old coordinates to the new ones
  QMap<double, double> xMap, yMap;
  for (double x : xSort) {
    xMap[x] = i;
    i++;
  }
  i = 1;
  for (double y : ySort) {
    yMap[y] = i;
    i++;
  }
  // Move every node to the new coordinate system
  coordsRev.clear();
  for (int index : coords.keys()) {
    QPointF pt = coords[index];
    pt = QPointF(xMap[pt.x()], yMap[pt.y()]);
    coords[index] = pt;
    if (coordsRev.contains(pointToPair(pt))) {
      qDebug() << "XXX: conflict" << index << coordsRev[pointToPair(pt)];
    }
    coordsRev[pointToPair(pt)] = index;
  }
  // Mark lattice points that are occupied by a path
  markPathPoints();
}

void MapLayout::markPathPoints()
{
  pathPoints.clear();
  for (auto [ roomId, startPoint ] : pairs(coords)) {
    const MapRoom* room = map->room(roomId);
    for (auto [ dir, exit ] : cpairs(room->exits)) {
      int destId = exit.dest;
      if (!coords.contains(destId)) {
        continue;
      }
      QPointF vec = dirVectors[dir];
      QPointF endPoint = coords[destId];
      int dx = endPoint.x() - startPoint.x();
      int dy = endPoint.y() - startPoint.y();
      if ((dx < 0) != (vec.x() < 0) || (dy < 0) != (vec.y() < 0)) {
        continue;
      }
      QPair<int, int> pp(startPoint.x() * 1024, startPoint.y() * 1024);
      if (dy == 0 && (dx < -1 || dx > 1)) {
        auto [minX, maxX] = in_order<int>(startPoint.x(), endPoint.x());
        for (int x = minX + 1; x <= maxX - 1; x++) {
          pp.first = x * 1024;
          pathPoints[pp] << roomId;
          pathPoints[pp] << destId;
        }
      } else if (dx == 0 && (dy < -1 || dy > 1)) {
        auto [minY, maxY] = in_order<int>(startPoint.y(), endPoint.y());
        for (int y = minY + 1; y <= maxY - 1; y++) {
          pp.second = y * 1024;
          pathPoints[pp] << roomId;
          pathPoints[pp] << destId;
        }
      } else if (qAbs(dx) > 1 && (dx == dy || dx == -dy)) {
        QPoint step(signum(dx), signum(dy));
        QPoint end = endPoint.toPoint();
        for (QPoint i = (startPoint + step).toPoint(); i != end; i += step) {
          pathPoints[pointToPair(i)] << roomId << destId;
        }
      }
    }
  }
}

void MapLayout::relax()
{
  bool rerun;
  int maxIter = 500;
  do {
    rerun = false;
    relattice();
    // Check for lines that cross other rooms
    for (auto [ moveRoomId, movePoint ] : pairs(coords)) {
      auto pair = pointToPair(movePoint);
      auto pp = pathPoints.value(pair);
      if (pp.isEmpty()) {
        continue;
      }
      const MapRoom* moveRoom = map->room(moveRoomId);
      // moveRoomId lies on the path between the rooms in pp
      for (int fromId : pp) {
        const MapRoom* fromRoom = map->room(fromId);
        for (const MapExit& exit : fromRoom->exits) {
          if (exit.dest == fromId || !pp.contains(exit.dest)) {
            continue;
          }
          if (exit.dest < fromId) {
            const MapRoom* toRoom = map->room(exit.dest);
            if (toRoom->hasExitTo(fromId)) {
              // Already checked this connection
              continue;
            }
          }
          // not a self-connection, and the other room is also connected to the same path segment
          QPointF startPoint = coords.value(fromId);
          QPointF endPoint = coords.value(exit.dest);
          if (startPoint.y() == endPoint.y()) {
            // Should the room move closer to its north neighbor or its south neighbor?
            int northId = moveRoom->exits["N"].dest, southId = moveRoom->exits["S"].dest;
            double northDist = 0, southDist = 0;
            if (northId > 0 && coords.contains(northId)) {
              northDist = startPoint.y() - coords[northId].y();
            }
            if (southId > 0 && coords.contains(southId)) {
              southDist = coords[southId].y() - startPoint.y();
            }
            if (!northDist && !southDist) {
              southDist = 1;
            }
            QPointF newPt = movePoint;
            if (northDist > southDist) {
              newPt.ry() -= 0.25;
            } else {
              newPt.ry() += 0.25;
            }
            coordsRev.remove(pair);
            coords[moveRoomId] = newPt;
            coordsRev[pointToPair(newPt)] = moveRoomId;
            rerun = true;
          } else if (startPoint.x() == endPoint.x()) {
            // Should the room move closer to its west neighbor or its east neighbor?
            int westId = moveRoom->exits["W"].dest, eastId = moveRoom->exits["E"].dest;
            double westDist = 0, eastDist = 0;
            if (westId > 0 && coords.contains(westId)) {
              westDist = startPoint.x() - coords[westId].x();
            }
            if (eastId > 0 && coords.contains(eastId)) {
              eastDist = coords[eastId].x() - startPoint.x();
            }
            if (!westDist && !eastDist) {
              eastDist = 1;
            }
            QPointF newPt = movePoint;
            if (westDist > eastDist) {
              newPt.rx() -= 0.25;
            } else {
              newPt.rx() += 0.25;
            }
            coordsRev.remove(pair);
            coords[moveRoomId] = newPt;
            coordsRev[pointToPair(newPt)] = moveRoomId;
            rerun = true;
          }
        }
      }
    }
  } while (rerun && --maxIter > 0);
  maxIter = 500;
  do {
    rerun = false;
    for (auto [ roomId, startPoint ] : pairs(coords)) {
      double initial = tension(roomId);
      if (!initial) {
        continue;
      }
      QMap<int, QPointF> substitutions;
      static const QPointF offsetSteps[] = {
        { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 },
        { -2, 0 }, { 2, 0 }, { 0, -2 }, { 0, 2 },
        { 1, 1 }, { -1, -1 },
      };
      QPointF bestPoint = startPoint;
      double bestTension = initial;
      for (const QPointF& offset : offsetSteps) {
        QPointF pt = startPoint + offset;
        auto pair = pointToPair(pt);
        if (coordsRev.contains(pair)) {
          continue;
        }
        auto pp = pathPoints.value(pair);
        if (pp.size() == 2 ? !pp.contains(roomId) : pp.size() > 2) {
          continue;
        }
        substitutions[roomId] = pt;
        double t = tension(roomId, substitutions);
        if (t == bestTension && (offset.x() < 0 || offset.y() < 0)) {
          bestTension = t;
          bestPoint = pt;
        } else if (t < bestTension || (t == bestTension && (offset.x() < 0 || offset.y() < 0))) {
          bestTension = t;
          bestPoint = pt;
        }
      }
      if (bestTension < initial || (bestTension == initial && bestPoint != startPoint)) {
        coordsRev.remove(pointToPair(startPoint));
        coords[roomId] = bestPoint;
        coordsRev[pointToPair(bestPoint)] = roomId;
        rerun = true;
        break;
      }
    }
    relattice();
  } while (rerun && --maxIter > 0);
  calculateBoundingBox();
}

QSize MapLayout::displaySize() const
{
  return boundingBox.size().toSize();
}

void MapLayout::render(QPainter* painter, const QRectF&, bool drawLabels) const
{
  int nl = 0;

  painter->save();
  painter->translate(-boundingBox.x(), -boundingBox.y());

  painter->save();
  painter->scale(COORD_SCALE, COORD_SCALE);
  painter->translate(-0.5, -0.5);

  for (const LayerData& layer : layers) {
    painter->setBrush(Qt::transparent);
    nl = (nl + 1) % nlColors.size();
    QColor c = nlColors[nl];
    c.setAlpha(128);
    painter->setPen(QPen(c, 0));
    c.setAlpha(8);
    painter->setBrush(c);
    painter->drawPolygon(layer.polygon);
  }

  painter->restore();

  painter->setFont(QFont("sans-serif", 2.0));
  painter->setPen(QPen(Qt::black, 0));
  for (int roomId : coords.keys()) {
    QPointF pos = coords[roomId] * COORD_SCALE;

    static const QPointF OFFSET(COORD_SCALE, COORD_SCALE);
    static const QSizeF SIZE(COORD_SCALE * 2, COORD_SCALE * 2);
    QRectF rect(pos - OFFSET, SIZE);

    const MapRoom* room = map->room(roomId);
    for (const QString& dir : room->exits.keys()) {
      int dest = room->exits[dir].dest;
      if (dest == roomId) {
        continue;
      }
      const MapRoom* other = map->room(dest);
      QString revDir = MapRoom::reverseDir(dir);
      bool oneWay = true;
      bool nonlinear = false;
      if (other) {
        for (const QString& rev : other->exits.keys()) {
          if (other->exits[rev].dest == roomId) {
            nonlinear = (revDir != rev);
            oneWay = false;
            revDir = rev;
            break;
          }
        }
      }
      if (other && coords.contains(dest)) {
        QPointF exitOffset = toEdge(dirVectors[dir]) * EXIT_OFFSET;
        QPointF start = pos + exitOffset;
        QPointF revExitOffset = toEdge(dirVectors[revDir]) * EXIT_OFFSET;
        QPointF end = coords.value(dest) * COORD_SCALE + revExitOffset;
        if (normalized(exitOffset) != normalized(end - start) || (start.x() != end.x() && start.y() != end.y() && magSqr(start - end) > 200)) {
          nonlinear = true;
          if (start.y() < end.y() || (start.y() == end.y() && start.x() < end.x())) {
            start -= oneWayOffset.value(dir) * ROOM_SIZE;
            end += oneWayOffset.value(revDir) * ROOM_SIZE;
          } else {
            start += oneWayOffset.value(dir) * ROOM_SIZE;
            end -= oneWayOffset.value(revDir) * ROOM_SIZE;
          }
        } else {
          painter->setPen(QPen(Qt::black, 0));
        }
        if (nonlinear) {
          static QMap<int, QColor> lineColors;
          int nlKey = (oneWay ? roomId : qMin(roomId, dest)) * 10 + dirOrder.indexOf(dir);
          if (!lineColors.contains(nlKey)) {
            lineColors[nlKey] = nlColors[lineColors.size() % nlColors.size()];
          }
          painter->setPen(QPen(lineColors[nlKey], 0));
        }
        if (oneWay) {
          QPointF slope = -normalized(dirVectors[revDir]);
          QPointF perp = QPointF(-slope.y(), slope.x()) * 0.5;
          QPolygonF arrow({ end, end + perp - slope * 2, end - perp - slope * 2 });
          painter->setBrush(painter->pen().color());
          painter->drawConvexPolygon(arrow);
          end = end - slope * 2;
        }
        if (nonlinear) {
          QPointF c1 = start + exitOffset * 4;
          QPointF c2 = end + revExitOffset * 4;
          if (c1.y() == c2.y()) {
            c1.setY(c1.y() + STEP_SIZE);
            c2.setY(c2.y() + STEP_SIZE);
          } else if (c1.x() == c2.x()) {
            c1.setX(c1.x() + STEP_SIZE);
            c2.setX(c2.x() + STEP_SIZE);
          }
          QPainterPath path(start);
          path.cubicTo(c1, c2, end);
          painter->setBrush(Qt::transparent);
          painter->drawPath(path);
        } else {
          painter->drawLine(start, end);
        }
      } else {
        painter->setPen(QPen(Qt::black, 0, Qt::DashLine));
        QPointF endpoint = pos + dirVectors[dir] * STEP_SIZE;
        painter->drawLine(pos, endpoint);
        if (other && other->zone != title) {
          painter->setBrush(Qt::white);
          painter->drawEllipse(QRectF(endpoint - QPoint(1.5, 1.5), endpoint + QPoint(1.5, 1.5)));
        }
        painter->setPen(QPen(Qt::black, 0));
      }
    }
  }

  painter->setPen(QPen(Qt::black, 0.5));
  for (int roomId : coords.keys()) {
    QPointF pos = coords[roomId] * COORD_SCALE;
    QRectF rect(pos - ROOM_OFFSET, ROOM_SIZEF);
    painter->setBrush(colors.value(roomId, Qt::white));
    painter->drawRect(rect.toRect());
    painter->setBrush(Qt::black);
    if (drawLabels) {
      painter->drawText(rect.toRect().adjusted(0, 0, 0, -1), Qt::AlignCenter, QString::number(roomId % 100));
    }
  }

  painter->restore();
}

const MapRoom* MapLayout::roomAt(const QPointF& _pt) const
{
  QPointF pt = (_pt + boundingBox.topLeft()) / COORD_SCALE;
  // Round to nearest quarter-point, bias away from zero
  pt.setX(int(pt.x() * 4 + 0.5 * signum(pt.x())) / 4.0);
  pt.setY(int(pt.y() * 4 + 0.5 * signum(pt.y())) / 4.0);
  QPair<int, int> rev = pointToPair(pt);
  int roomId = coordsRev.value(rev, -1);
  if (roomId > 0) {
    return map->room(roomId);
  }
  return nullptr;
}

QRectF MapLayout::roomPos(int roomId) const
{
  if (!coords.contains(roomId)) {
    return QRectF();
  }
  QPointF pos = (coords[roomId] * COORD_SCALE) - boundingBox.topLeft();
  QRectF rect(pos - ROOM_OFFSET, ROOM_SIZEF);
  return rect;
}

void MapLayout::calculateRegion(LayerData& layer)
{
  QRegion r;
  for (auto [ roomId, pos ] : cpairs(layer.coords)) {
    bool hasExit = false;
    for (const auto& [ dir, exit ] : cpairs(map->room(roomId)->exits)) {
      if (!layer.coords.contains(exit.dest)) {
        continue;
      }
      hasExit = true;
      QPointF destPos = layer.coords.value(exit.dest);
      int x0 = qMin(pos.x(), destPos.x());
      int x1 = qMax(pos.x(), destPos.x()) + 1;
      int y0 = qMin(pos.y(), destPos.y());
      int y1 = qMax(pos.y(), destPos.y()) + 1;
      r += QRect(x0, y0, x1 - x0, y1 - y0);
    }
    if (!hasExit) {
      r += QRect(pos.x(), pos.y(), 1, 1);
    }
  }
  QRect bb = r.boundingRect();
  QRegion add;
  for (int y = bb.top(); y <= bb.bottom(); y++) {
    for (int x = bb.left(); x <= bb.right(); x++) {
      if (r.contains(QPoint(x, y))) {
        continue;
      }
      if ((r.contains(QPoint(x - 1, y)) && r.contains(QPoint(x + 1, y))) ||
          (r.contains(QPoint(x, y - 1)) && r.contains(QPoint(x, y + 1)))) {
        r += QRect(x, y, 1, 1);
      }
    }
  }
  r += add;
  layer.region = r;

  QPainterPath path;
  for (const QRect& rect : r) {
    QPainterPath p;
    p.setFillRule(Qt::WindingFill);
    // Grow the rects by an imperceptible amount to ensure they're overlapping
    p.addRect(QRectF(rect).adjusted(-0.000001, -0.000001, 0.000001, 0.000001));
    path = path.united(p);
  }

  layer.polygon.clear();
  for (const QPolygonF& poly : path.toSubpathPolygons()) {
    layer.polygon = layer.polygon.united(poly);
  }

  // At this point the polygon's points are in clockwise winding order
  int ct = layer.polygon.size() - 1;
  QPolygonF reduced;
  for (int i = 0; i < ct; i++) {
    QPointF prev = layer.polygon[(i + ct - 1) % ct];
    QPointF pt = layer.polygon[i];
    QPointF next = layer.polygon[(i + 1) % ct];
    const QPointF& perp1 = normalized(QPointF(prev.y() - pt.y(), pt.x() - prev.x()));
    const QPointF& perp2 = normalized(QPointF(pt.y() - next.y(), next.x() - pt.x()));
    if (perp1 == perp2) {
      // collinear point, remove
      continue;
    }
    reduced << (pt + (perp1 + perp2) * 0.1);
  }
  reduced << reduced.first();
  layer.polygon = reduced;

  layer.boundingBox = layer.region.boundingRect();
}

MapLayout::LayerData* MapLayout::findLayer(int roomId)
{
  for (LayerData& layer : layers) {
    if (layer.coords.contains(roomId)) {
      return &layer;
    }
  }
  return nullptr;
}
