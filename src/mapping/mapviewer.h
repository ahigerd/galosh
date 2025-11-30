#ifndef GALOSH_MAPVIEWER_H
#define GALOSH_MAPVIEWER_H

#include <QWidget>
#include <memory>
#include "maplayout.h"
class QScrollArea;
class QComboBox;
class MapManager;
class MapWidget;
class ExploreHistory;

class MapViewer : public QWidget
{
Q_OBJECT
public:
  enum MapType {
    MiniMap,
    EmbedMap,
    StandaloneMap,
  };

  MapViewer(MapType mapType, MapManager* map, ExploreHistory* history, QWidget* parent = nullptr);

public slots:
  void reload();
  void loadZone(const QString& name, bool force = false);
  void setZoom(double level);
  void zoomIn();
  void zoomOut();

  void setCurrentRoom(int roomId);

signals:
  void exploreMap(int roomId);

protected slots:
  void repositionHeader();

protected:
  // TODO: explore on double-click
  // TODO: menu on right-click
  void showEvent(QShowEvent*);
  void moveEvent(QMoveEvent*);
  void resizeEvent(QResizeEvent*);

private:
  friend class MapWidget;

  std::unique_ptr<MapLayout> mapLayout;
  MapManager* map;
  QWidget* header;
  QScrollArea* scrollArea;
  MapWidget* view;
  QComboBox* zone;
  MapType mapType;
};

#endif
