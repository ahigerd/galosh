#ifndef EXPLOREDIALOG_H
#define EXPLOREDIALOG_H

#include <QDialog>
class MapManager;
class RoomView;
class QLineEdit;
class QPushButton;

class ExploreDialog : public QDialog
{
Q_OBJECT
public:
  ExploreDialog(MapManager* map, int roomId, int lastRoomId, QWidget* parent = nullptr);

  inline RoomView* roomView() const { return room; }

signals:
  void exploreRoom(int roomId);

public slots:
  void roomUpdated(const QString& title, int id);
  void refocus();

private slots:
  void doCommand();
  void goBack();
  void setErrorState(bool on = false);

private:
  MapManager* map;
  RoomView* room;
  QLineEdit* line;
  QPushButton* backButton;
  QList<int> roomHistory;
};

#endif
