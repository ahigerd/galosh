#ifndef GALOSH_ROOMVIEW_H
#define GALOSH_ROOMVIEW_H

#include <QWidget>
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QGroupBox;
class MapManager;

class RoomView : public QWidget
{
Q_OBJECT
public:
  RoomView(QWidget* parent = nullptr);
  RoomView(MapManager* map, int roomId, int lastRoomId, QWidget* parent = nullptr);

  QSize sizeHint() const override;
  int exitDestination(const QString& dir) const;

public slots:
  void setRoom(MapManager* map, int roomId);

private slots:
  void exploreRoom(QListWidgetItem* item);

signals:
  void roomUpdated(const QString& title, int roomId);
  void exploreRoom(int roomId);

private:
  QLabel* roomDesc;
  QGroupBox* exitBox;
  QListWidget* exits;
  MapManager* mapManager;
  int currentRoomId;
  int lastRoomId;
};

#endif
