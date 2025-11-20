#ifndef GALOSH_MAPVIEWER_H
#define GALOSH_MAPVIEWER_H

#include <QScrollArea>
#include <memory>
#include "maplayout.h"
class QComboBox;
class MapManager;
class MapWidget;

class MapViewer : public QScrollArea
{
Q_OBJECT
public:
  MapViewer(MapManager* map, QWidget* parent = nullptr);

public slots:
  void loadZone(const QString& name);
  void setZoom(double level);
  void zoomIn();
  void zoomOut();

  void setCurrentRoom(int roomId);

  // TODO: toggle pin

signals:
  void exploreMap(int roomId);

private slots:
  inline void setCurrentRoom(MapManager*, int roomId) { setCurrentRoom(roomId); }

protected:
  // TODO: explore on double-click
  // TODO: menu on right-click
  void moveEvent(QMoveEvent*);
  void resizeEvent(QResizeEvent*);

private:
  friend class MapWidget;

  std::unique_ptr<MapLayout> mapLayout;
  MapManager* map;
  QWidget* header;
  MapWidget* view;
  QComboBox* zone;
};

#endif
