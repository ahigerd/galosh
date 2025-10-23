#ifndef GALOSH_ROOMVIEW_H
#define GALOSH_ROOMVIEW_H

#include <QWidget>
class QLabel;
class QListWidget;
class QGroupBox;
class MapManager;

class RoomView : public QWidget
{
Q_OBJECT
public:
  RoomView(QWidget* parent = nullptr);

public slots:
  void setRoom(MapManager* map, int roomId);

private:
  QGroupBox* roomBox;
  QLabel* roomDesc;
  QListWidget* exits;
};

#endif
