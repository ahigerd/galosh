#ifndef GALOSH_GALOSHWINDOW_H
#define GALOSH_GALOSHWINDOW_H

#include <QMainWindow>
#include "triggermanager.h"
#include "mapmanager.h"
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
  void openProfileDialog(bool forConnect = false);

protected:
  void showEvent(QShowEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

private slots:
  void connectToProfile(const QString& profilePath);
  void updateStatus();
  void gmcpEvent(const QString& key, const QVariant& value);

private:
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
