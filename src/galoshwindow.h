#ifndef GALOSH_GALOSHWINDOW_H
#define GALOSH_GALOSHWINDOW_H

#include <QMainWindow>
#include "triggermanager.h"
#include "mapmanager.h"
#include "profiledialog.h"
class QLabel;
class QTreeView;
class GaloshTerm;
class InfoModel;
class RoomView;

class GaloshWindow : public QMainWindow
{
Q_OBJECT
public:
  GaloshWindow(QWidget* parent = nullptr);

public slots:
  void openConnectDialog();
  void openProfileDialog(ProfileDialog::Tab tab);

protected:
  void showEvent(QShowEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

private slots:
  void connectToProfile(const QString& profilePath);
  void reloadProfile(const QString& profilePath);
  void updateStatus();
  void gmcpEvent(const QString& key, const QVariant& value);

private:
  QString currentProfile;
  TriggerManager triggers;
  MapManager map;
  GaloshTerm* term;
  QTreeView* infoView;
  InfoModel* infoModel;
  RoomView* roomView;
  QLabel* sbStatus;
  bool fixGeometry;
};

#endif
