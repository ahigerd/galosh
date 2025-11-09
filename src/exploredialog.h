#ifndef GALOSH_EXPLOREDIALOG_H
#define GALOSH_EXPLOREDIALOG_H

#include <QDialog>
#include "explorehistory.h"
class MapManager;
class RoomView;
class CommandLine;
class QPushButton;

class ExploreDialog : public QDialog
{
Q_OBJECT
public:
  ExploreDialog(MapManager* map, int roomId, int lastRoomId, const QString& movement, QWidget* parent = nullptr);

  inline RoomView* roomView() const { return room; }

signals:
  void exploreRoom(int roomId, const QString& movement);

public slots:
  void roomUpdated(const QString& title, int id, const QString& movement = QString());
  void refocus();

private slots:
  void doCommand();
  void setResponse(bool isError = false, const QString& message = QString());
  void clearResponse();

  void do_BACK();
  void do_HELP();
  void do_GOTO(const QStringList&);
  inline void do_ZONES() { return do_ZONE({}); }
  void do_ZONE(const QStringList&);
  void do_SEARCH(const QStringList&);
  void do_HISTORY(const QStringList&);
  void do_REVERSE(const QStringList&);
  void do_SPEED(const QStringList&);
  void do_SIMPLIFY(const QStringList&);
  void do_ROUTE(const QStringList&);
  void do_RESET();

private:
  void handleCommand(const QString& command, const QStringList& args);

  MapManager* map;
  RoomView* room;
  CommandLine* line;
  QPushButton* backButton;
  ExploreHistory history;
};

#endif
