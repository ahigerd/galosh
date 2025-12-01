#ifndef GALOSH_MAPVIEWER_H
#define GALOSH_MAPVIEWER_H

#include <QWidget>
#include <memory>
class QScrollArea;
class QComboBox;
class MapManager;
class MapWidget;
class MapLayout;
class ExploreHistory;
class GaloshSession;

class MapViewer : public QWidget
{
Q_OBJECT
public:
  enum MapType {
    MiniMap,
    EmbedMap,
    StandaloneMap,
  };

  MapViewer(MapType mapType, QWidget* parent = nullptr);

  void setSession(GaloshSession* session);

public slots:
  void reload();
  void loadZone(const QString& name, bool force = false);
  void setZoom(double level);
  void zoomIn();
  void zoomOut();

  void setCurrentRoom(int roomId = -1);

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

  GaloshSession* session;
  MapManager* map;
  MapLayout* mapLayout;
  QWidget* header;
  QScrollArea* scrollArea;
  MapWidget* view;
  QComboBox* zone;
  MapType mapType;
};

#endif
