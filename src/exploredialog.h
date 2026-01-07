#ifndef GALOSH_EXPLOREDIALOG_H
#define GALOSH_EXPLOREDIALOG_H

#include <QMainWindow>
#include "commands/textcommandprocessor.h"
#include "explorehistory.h"
#include "profiledialog.h"
class GaloshSession;
class MapManager;
class MapViewer;
class RoomView;
class CommandLine;
class QSplitter;
class QPushButton;
class QLabel;

class ExploreDialog : public QMainWindow, public TextCommandProcessor
{
Q_OBJECT
public:
  ExploreDialog(GaloshSession* session, int roomId, int lastRoomId, const QString& movement, QWidget* parent = nullptr);

  inline RoomView* roomView() const { return room; }

signals:
  void exploreRoom(int roomId, const QString& movement = QString());
  void openProfileDialog(ProfileDialog::Tab tab);

public slots:
  void roomUpdated(const QString& title, int id, const QString& movement = QString());
  void refocus();
  void togglePin();
  void showHelp();
  void openMapOptions();

private slots:
  void saveState();

  void doCommand();
  void setResponse(bool isError = false, const QString& message = QString());
  void clearResponse();

  void goBack();
  void goToRoom(const QString& id);
  void handleSpeedwalk(const QStringList& dirs);

  void openWaypoints();

protected:
  virtual void showCommandMessage(TextCommand* command, const QString& message, MessageType msgType) override;
  virtual bool commandFilter(const QString& command, const QStringList& args) override;
  virtual bool isCustomCommand(const QString& command) const override;

  void moveEvent(QMoveEvent*);
  void resizeEvent(QResizeEvent*);

private:
  QSplitter* splitter;
  GaloshSession* session;
  MapManager* map;
  MapViewer* mapView;
  QLabel* roomTitle;
  RoomView* room;
  CommandLine* line;
  QPushButton* backButton;
  QAction* pinAction;
  ExploreHistory history;
  QStringList responseLines;
  bool responseError;
  bool expectResponse;
};

#endif
