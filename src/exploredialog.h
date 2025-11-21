#ifndef GALOSH_EXPLOREDIALOG_H
#define GALOSH_EXPLOREDIALOG_H

#include <QDialog>
#include "commands/textcommandprocessor.h"
#include "explorehistory.h"
class MapManager;
class MapViewer;
class QSplitter;
class RoomView;
class CommandLine;
class QPushButton;

class ExploreDialog : public QDialog, public TextCommandProcessor
{
Q_OBJECT
public:
  ExploreDialog(MapManager* map, int roomId, int lastRoomId, const QString& movement, QWidget* parent = nullptr);

  inline RoomView* roomView() const { return room; }

signals:
  void exploreRoom(int roomId, const QString& movement = QString());

public slots:
  void roomUpdated(const QString& title, int id, const QString& movement = QString());
  void refocus();
  // TODO: toggle pin
  // TODO: open map

private slots:
  void saveState();

  void doCommand();
  void setResponse(bool isError = false, const QString& message = QString());
  void clearResponse();

  void goBack();
  void goToRoom(const QString& id);

protected:
  virtual void showCommandMessage(TextCommand* command, const QString& message, bool isError) override;
  virtual bool commandFilter(const QString& command, const QStringList& args) override;

  void moveEvent(QMoveEvent*);
  void resizeEvent(QResizeEvent*);

private:
  void handleSpeedwalk(const QStringList& dirs);

  QSplitter* splitter;
  MapManager* map;
  MapViewer* mapView;
  RoomView* room;
  CommandLine* line;
  QPushButton* backButton;
  ExploreHistory history;
  QStringList responseLines;
  bool responseError;
  bool expectResponse;
};

#endif
