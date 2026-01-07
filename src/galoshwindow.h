#ifndef GALOSH_GALOSHWINDOW_H
#define GALOSH_GALOSHWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QTimer>
#include <QMap>
#include "commands/textcommandprocessor.h"
#include "triggermanager.h"
#include "mapmanager.h"
#include "itemdatabase.h"
#include "profiledialog.h"
#include "explorehistory.h"
class QLabel;
class QTreeView;
class QStackedWidget;
class GaloshSession;
class InfoModel;
class RoomView;
class MapViewer;
class ExploreDialog;

class GaloshWindow : public QMainWindow
{
Q_OBJECT
public:
  GaloshWindow(QWidget* parent = nullptr);

  bool eventFilter(QObject* obj, QEvent* event);

public slots:
  void openConnectDialog();
  void openProfileDialog(ProfileDialog::Tab tab = ProfileDialog::Tab_Server);
  void openMapOptions();
  void openMsspDialog();
  void openConfigFolder();
  void openWebsite();
  void exploreMap(int roomId = -1, const QString& movement = QString());
  void openItemDatabase();
  void openItemSets();
  bool sendCommandToProfile(const QString& profile, const QString& command);
  void about();

protected:
  void showEvent(QShowEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void moveEvent(QMoveEvent*) override;
  void resizeEvent(QResizeEvent*) override;

private slots:
  void help();
  void reconnectSession();
  void disconnectSession();
  void connectToProfile(const QString& profilePath, bool online);
  void reloadProfile(const QString& profilePath);
  void closeSession(int index = -1);
  void updateStatus();
  void msspReceived();
  void updateGeometry(bool queue = false);
  void toggleRoomDock(bool checked);
  void toggleInfoDock(bool checked);
  void toggleMapDock(bool checked);
  void toggleParsing(bool checked);
  void sessionDestroyed(QObject* obj);

private:
  GaloshSession* session() const;
  GaloshSession* findSession(const QString& profilePath) const;
  bool confirmClose(GaloshSession* sess = nullptr);
  void updateActions();

  QTimer stateThrottle;
  QStackedWidget* stackedWidget;
  QTabWidget* tabs;
  QWidget* background;
  QMap<QWidget*, QPointer<GaloshSession>> sessions;
  QList<QAction*> profileActions;
  QList<QAction*> disconnectedActions;
  QList<QAction*> connectedActions;
  QDockWidget* infoDock;
  QAction* parsingAction;
  QAction* infoAction;
  QTreeView* infoView;
  QDockWidget* roomDock;
  QAction* roomAction;
  RoomView* roomView;
  QDockWidget* mapDock;
  QAction* mapDockAction;
  MapViewer* mapDockView;
  QAction* msspButton;
  QAction* msspMenu;
  QLabel* sbStatus;
  bool fixGeometry;
  bool geometryReady;
  bool shouldRestoreDocks;
};

#endif
