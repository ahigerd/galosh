#ifndef GALOSH_MAPVIEWER_H
#define GALOSH_MAPVIEWER_H

#include <QScrollArea>
#include <memory>
#include "maplayout.h"
class QComboBox;
class MapManager;
class MapWidget;
class ExploreHistory;

class MapViewer : public QScrollArea
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

  // TODO: toggle map follow
  // TODO: follow game or explore
  // TODO: toggle pin

signals:
  void exploreMap(int roomId);

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
  MapWidget* view;
  QComboBox* zone;
  MapType mapType;
};

#endif
