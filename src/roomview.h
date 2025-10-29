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

  QSize sizeHint() const override;

public slots:
  void setRoom(MapManager* map, int roomId);

signals:
  void roomUpdated(const QString& title);

private:
  QLabel* roomDesc;
  QGroupBox* exitBox;
  QListWidget* exits;
};

#endif
