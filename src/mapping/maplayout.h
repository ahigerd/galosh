#ifndef GALOSH_MAPLAYOUT_H
#define GALOSH_MAPLAYOUT_H

#include <QMap>
#include <QHash>
#include <QRectF>
#include <QPointF>
#include <QColor>
#include <memory>
#include "mapsearch.h"
class QPainter;
class MapManager;
class MapZone;
class MapRoom;

class MapLayout
{
public:
  MapLayout(MapManager* map, MapSearch* search = nullptr);

  void loadZone(const MapZone* zone);

  QSize displaySize() const;
  void render(QPainter* painter, const QRectF& viewport) const;

  const MapRoom* roomAt(const QPointF& pt) const;

private:
  void loadClique(const MapSearch::Clique* clique, int roomId);
  void relattice();
  void relax();
  void calculateBoundingBox();

  double tension(int roomId, int destRoomId, const QString& dir, const QMap<int, QPointF>& substitutions = {}) const;
  double tension(int roomId, const QMap<int, QPointF>& substitutions = {}) const;
  double tension(const QMap<int, QPointF>& substitutions = {}) const;

  struct CliqueData {
    QMap<int, QPointF> coords;
    QRectF boundingBox;
  };

  QString title;
  QRectF boundingBox;
  QMap<int, QPointF> coords;
  QHash<QPair<int, int>, int> coordsRev;
  QMap<QPair<int, int>, QSet<int>> pathPoints;
  QMap<int, QSet<int>> oneWayExits;
  QList<CliqueData> cliques;
  QMap<int, QColor> colors;
  MapManager* map;
  MapSearch* search;
  std::unique_ptr<MapSearch> ownedSearch;
};

#endif
