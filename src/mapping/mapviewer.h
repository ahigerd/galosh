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

protected:
  void resizeEvent(QResizeEvent*);

private:
  std::unique_ptr<MapLayout> mapLayout;
  MapManager* map;
  QWidget* header;
  MapWidget* view;
  QComboBox* zone;
};

#endif
