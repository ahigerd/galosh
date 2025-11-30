#ifndef GALOSH_GALOSHWINDOW_H
#define GALOSH_GALOSHWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QTimer>
#include "commands/textcommandprocessor.h"
#include "triggermanager.h"
#include "mapmanager.h"
#include "itemdatabase.h"
#include "profiledialog.h"
#include "explorehistory.h"
class QLabel;
class QTreeView;
class GaloshTerm;
class InfoModel;
class RoomView;
class MapViewer;
class ExploreDialog;

class GaloshWindow : public QMainWindow, public TextCommandProcessor
{
Q_OBJECT
public:
  GaloshWindow(QWidget* parent = nullptr);

  bool eventFilter(QObject* obj, QEvent* event);

public slots:
  void openConnectDialog();
  void openProfileDialog(ProfileDialog::Tab tab = ProfileDialog::ServerTab);
  void openMapOptions();
  void openMsspDialog();
  void openConfigFolder();
  void openWebsite();
  void showMap();
  void exploreMap(int roomId = -1, const QString& movement = QString());
  void about();

protected:
  void showEvent(QShowEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void moveEvent(QMoveEvent*) override;
  void resizeEvent(QResizeEvent*) override;

private slots:
  void connectToProfile(const QString& profilePath, bool online);
  void reloadProfile(const QString& profilePath);
  void updateStatus();
  void msspEvent(const QString&, const QString&);
  void gmcpEvent(const QString& key, const QVariant& value);
  void setLastRoom(const QString& title, int roomId);
  void updateGeometry(bool queue = false);
#ifdef Q_MOC_RUN
  void help();
  void handleCommand(const QString& command, const QStringList& args);
#endif
  void speedwalk(const QStringList& steps);
  void toggleRoomDock(bool checked);
  void toggleInfoDock(bool checked);
  void toggleMapDock(bool checked);
  void abortSpeedwalk();
  void showCommandMessage(TextCommand* command, const QString& message, bool isError) override;

private:
  bool msspAvailable() const;

  QTimer stateThrottle;
  QString currentProfile;
  TriggerManager triggers;
  MapManager map;
  ExploreHistory exploreHistory;
  ItemDatabase itemDB;
  GaloshTerm* term;
  QList<QAction*> profileActions;
  QDockWidget* infoDock;
  QAction* infoAction;
  QTreeView* infoView;
  InfoModel* infoModel;
  QDockWidget* roomDock;
  QAction* roomAction;
  RoomView* roomView;
  QDockWidget* mapDock;
  QAction* mapDockAction;
  MapViewer* mapDockView;
  QAction* msspButton;
  QAction* msspMenu;
  QLabel* sbStatus;
  QPointer<ExploreDialog> explore;
  QPointer<MapViewer> mapView;
  QStringList speedPath;
  int lastRoomId;
  bool fixGeometry;
  bool geometryReady;
  bool shouldRestoreDocks;
};

#endif
