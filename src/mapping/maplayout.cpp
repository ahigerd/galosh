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
  { "N", QPointF(0.25, 0) },
  { "E", QPointF(0, 0.25) },
  { "S", QPointF(-0.25, 0) },
  { "W", QPointF(0, -0.25) },
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

static QPointF normalized(const QPointF& pt)
{
  if (pt.isNull()) {
    return pt;
  }
  return pt / std::sqrt(pt.x() * pt.x() + pt.y() * pt.y());
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

/*
static QPointF toLattice(const QPointF& pt)
{
  double x = pt.x();
  double y = pt.y();
  x = (x < 0 ? -1 : (x > 0 ? 1 : 0));
  y = (y < 0 ? -1 : (y > 0 ? 1 : 0));
  return QPointF(x, y);
}
*/

MapLayout::MapLayout(MapManager* map, MapSearch* search)
: map(map), search(search)
{
  // initializers only
}

void MapLayout::loadZone(const MapZone* zone)
{
  Q_ASSERT(zone);
  cliques.clear();
  colors.clear();
  if (!search) {
    search = new MapSearch(map);
    ownedSearch.reset(search);
    search->precompute();
  }

  for (const MapSearch::Clique* clique : search->cliquesForZone(zone)) {
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
    loadClique(clique, roomId);
    relax();
    cliques << (CliqueData){ coords, boundingBox };
  }

  coords.clear();
  coordsRev.clear();
  boundingBox = QRectF();
  QPointF offset;
  for (const CliqueData& clique : cliques) {
    for (int roomId : clique.coords.keys()) {
      QPointF pos = clique.coords[roomId] + offset;
      coords[roomId] = pos;
      coordsRev[pointToPair(pos)] = roomId;
    }
    offset.rx() += clique.boundingBox.width();
  }

  calculateBoundingBox();
  boundingBox.adjust(-1, -1, 0, 0);
  boundingBox = QRectF(boundingBox.topLeft() * COORD_SCALE, boundingBox.bottomRight() * COORD_SCALE);

  title = zone->name;

  /*
  static QTabWidget* tabs = new QTabWidget;
  tabs->show();

  QRectF viewport(boundingBox.topLeft() * RENDER_SCALE * COORD_SCALE, boundingBox.bottomRight() * RENDER_SCALE * COORD_SCALE);
  QImage img(viewport.size().toSize(), QImage::Format_RGB32);
  img.fill(0xFF808080);
  {
    QPainter p(&img);
    p.translate(-viewport.topLeft());
    p.scale(RENDER_SCALE, RENDER_SCALE);
    render(&p, viewport);
  }
  QScrollArea* sa = new QScrollArea;
  QLabel* l = new QLabel(sa);
  l->setPixmap(QPixmap::fromImage(img));
  sa->setWidget(l);
  tabs->addTab(sa, title);
  */
}

void MapLayout::loadClique(const MapSearch::Clique* clique, int roomId)
{
  const MapRoom* room = map->room(roomId);
  for (const QString& dir : room->exits.keys()) {
    QPointF pos = coords.value(roomId);
    int dest = room->exits[dir].dest;
    if (dir == "U" || dir == "D") {
      const MapRoom* destRoom = map->room(dest);
      if (!destRoom) {
        continue;
      }
      if (destRoom->exits.size() > 1 || destRoom->exits.value(MapRoom::reverseDir(dir)).dest != roomId) {
        // TODO: layers
        //continue;
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
    loadClique(clique, dest);
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

double MapLayout::tension(int roomId, const QMap<int, QPointF>& substitutions) const
{
  double total = 0;
  QPointF startPoint = substitutions.value(roomId, coords[roomId]);
  const MapRoom* room = map->room(roomId);
  for (auto [ dir, exit ] : cpairs(room->exits)) {
    if (dir == "U" || dir == "D") {
      // up/down exits don't contribute to tension right now
      //continue;
    }
    if (!coords.contains(exit.dest)) {
      continue;
    }
    const MapRoom* dest = map->room(exit.dest);
    QPointF endPoint = substitutions.value(exit.dest, coords[exit.dest]);
    QString reverse = dest->findExit(roomId);
    if (reverse.isEmpty()) {
      reverse = MapRoom::reverseDir(dir);
    }
    QPointF step = startPoint + dirVectors[dir] * 0.25;
    QPointF revStep = endPoint + dirVectors[reverse] * 0.25;
    double dx = step.x() - revStep.x() + 0.2;
    double dy = step.y() - revStep.y();
    if (dir == "U" || dir == "D") {
      dy -= dx;
      dx *= 1.5;
    }
    double tension = std::sqrt((dx * dx) + (dy * dy));
    if (tension <= 1) {
      tension = 0;
    }
    if (step.x() != revStep.x() && step.y() != revStep.y()) {
      // nonlinearity introduces a lot of extra tension
      // but past a certain point it stops being relevant
      if (tension > 3) {
        tension = 60 + 2 * std::sqrt(tension - 3);
      } else {
        tension *= 20.0;
      }
    }
    total += tension;
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
      }
    }
  }
}

void MapLayout::relax()
{
  bool rerun;
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
  } while (rerun);
  do {
    rerun = false;
    for (auto [ roomId, startPoint ] : pairs(coords)) {
      if (!tension(roomId)) {
        continue;
      }
      double initial = tension();
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
        double t = tension(substitutions);
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
  } while (rerun);
  calculateBoundingBox();
}

QSize MapLayout::displaySize() const
{
  return boundingBox.size().toSize();
}

void MapLayout::render(QPainter* painter, const QRectF& rawViewport) const
{
  int nl = 0;
  QRectF viewport = rawViewport;
  viewport.moveLeft(-boundingBox.left());
  viewport.moveTop(-boundingBox.top());
  painter->save();
  painter->setFont(QFont("sans-serif", 2.0));
  painter->setPen(QPen(Qt::black, 0));
  for (int roomId : coords.keys()) {
    QPointF pos = coords[roomId] * COORD_SCALE;

    static const QPointF OFFSET(COORD_SCALE, COORD_SCALE);
    static const QSizeF SIZE(COORD_SCALE * 2, COORD_SCALE * 2);
    QRectF rect(pos - OFFSET, SIZE);
    if (!viewport.intersects(rect)) {
      continue;
    }

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
        if (normalized(exitOffset) != normalized(end - start)) {
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
          nl = (nl + 1) % nlColors.size();
          painter->setPen(QPen(nlColors[nl], 0));
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
        painter->setPen(QPen(Qt::black, 0));
        if (other && other->zone != title) {
          painter->drawText(QRectF(endpoint - QPoint(5, 2), endpoint + QPoint(5, 0)), Qt::TextDontClip | Qt::AlignCenter, other->zone);
        }
      }
    }
  }

  painter->setPen(QPen(Qt::black, 0.5));
  for (int roomId : coords.keys()) {
    QPointF pos = coords[roomId] * 15;
    QRectF rect(pos - ROOM_OFFSET, ROOM_SIZEF);
    if (!viewport.intersects(rect)) {
      continue;
    }
    painter->setBrush(colors.value(roomId, Qt::white));
    painter->drawRect(rect.toRect());
    painter->setBrush(Qt::black);
    painter->drawText(rect.toRect().adjusted(0, 0, 0, -1), Qt::AlignCenter, QString::number(roomId % 100));
  }

  painter->restore();
}

const MapRoom* MapLayout::roomAt(const QPointF& pt) const
{
  QPair<int, int> rev = pointToPair(pt / COORD_SCALE);
  rev.first = ((rev.first + 128) >> 8) << 8;
  rev.second = ((rev.second + 128) >> 8) << 8;
  int roomId = coordsRev.value(rev, -1);
  if (roomId > 0) {
    return map->room(roomId);
  }
  return nullptr;
}
