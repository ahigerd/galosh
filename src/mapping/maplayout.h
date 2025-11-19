#ifndef GALOSH_MAPLAYOUT_H
#define GALOSH_MAPLAYOUT_H

#include <QMap>
#include <QHash>
#include <QRectF>
#include <QPointF>
#include <QPolygonF>
#include <QRegion>
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
  struct LayerData {
    const MapSearch::Clique* source;
    QMap<int, QPointF> coords;
    QRectF boundingBox;
    int zIndex;
    QRegion region;
    QPolygonF polygon;
    QSet<QPair<int, int>> layerExits;
  };

  void loadClique(const MapSearch::Clique* clique, int roomId, int zIndex);
  void relattice();
  void relax();
  void calculateBoundingBox();
  void calculateRegion(LayerData& layer);

  double tension(int roomId, int destRoomId, const QString& dir, const QMap<int, QPointF>& substitutions = {}, bool weightHigh = false) const;
  double tension(int roomId, const QMap<int, QPointF>& substitutions = {}, bool weightHigh = false) const;
  double tension(const QMap<int, QPointF>& substitutions = {}) const;

  LayerData* findLayer(int roomId);

  QString title;
  QRectF boundingBox;
  QMap<int, QPointF> coords;
  QMap<int, int> roomLayers;
  QHash<QPair<int, int>, int> coordsRev;
  QMap<QPair<int, int>, QSet<int>> pathPoints;
  QMap<int, QSet<int>> oneWayExits;
  QList<LayerData> layers;
  QMap<int, int> pendingLayers;
  QMap<int, QColor> colors;
  MapManager* map;
  MapSearch* search;
  std::unique_ptr<MapSearch> ownedSearch;
};

#endif
