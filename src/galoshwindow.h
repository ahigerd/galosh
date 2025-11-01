#ifndef GALOSH_GALOSHWINDOW_H
#define GALOSH_GALOSHWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QTimer>
#include "triggermanager.h"
#include "mapmanager.h"
#include "profiledialog.h"
class QLabel;
class QTreeView;
class GaloshTerm;
class InfoModel;
class RoomView;
class ExploreDialog;

class GaloshWindow : public QMainWindow
{
Q_OBJECT
public:
  GaloshWindow(QWidget* parent = nullptr);

  bool eventFilter(QObject* obj, QEvent* event);

public slots:
  void openConnectDialog();
  void openProfileDialog(ProfileDialog::Tab tab = ProfileDialog::ServerTab);
  void openMsspDialog();
  void openConfigFolder();
  void openWebsite();
  void exploreMap(int roomId = -1);
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
  void toggleRoomDock(bool checked);
  void toggleInfoDock(bool checked);

private:
  bool msspAvailable() const;

  QTimer stateThrottle;
  QString currentProfile;
  TriggerManager triggers;
  MapManager map;
  GaloshTerm* term;
  QAction* exploreAction;
  QDockWidget* infoDock;
  QAction* infoAction;
  QTreeView* infoView;
  InfoModel* infoModel;
  QDockWidget* roomDock;
  QAction* roomAction;
  RoomView* roomView;
  QAction* msspButton;
  QAction* msspMenu;
  QLabel* sbStatus;
  QPointer<ExploreDialog> explore;
  int lastRoomId;
  bool fixGeometry;
  bool geometryReady;
};

#endif
