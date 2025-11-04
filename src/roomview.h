#ifndef GALOSH_ROOMVIEW_H
#define GALOSH_ROOMVIEW_H

#include <QWidget>
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTextBrowser;
class QGroupBox;
class MapManager;
class MapRoom;

class RoomView : public QWidget
{
Q_OBJECT
public:
  static QString formatRoomTitle(const MapRoom* room);

  RoomView(QWidget* parent = nullptr);
  RoomView(MapManager* map, int roomId, int lastRoomId, QWidget* parent = nullptr);

  QSize sizeHint() const override;
  int exitDestination(const QString& dir) const;

public slots:
  void setRoom(MapManager* map, int roomId, const QString& movement = QString());
  void setResponseMessage(const QString& message, bool isError);

private slots:
  void exploreRoom(QListWidgetItem* item);

signals:
  void roomUpdated(const QString& title, int roomId, const QString& movement = QString());
  void exploreRoom(int roomId, const QString& movement = QString());

private:
  QLabel* roomDesc;
  QTextBrowser* responseMessage;
  QGroupBox* exitBox;
  QListWidget* exits;
  MapManager* mapManager;
  int currentRoomId;
  int lastRoomId;
};

#endif
