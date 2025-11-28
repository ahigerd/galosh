#ifndef GALOSH_MAPOPTIONS_H
#define GALOSH_MAPOPTIONS_H

#include <QDialog>
class MapManager;
class QTableWidget;

class MapOptions : public QDialog
{
Q_OBJECT
public:
  MapOptions(MapManager* map, QWidget* parent = nullptr);

  void done(int r) override;

public slots:
  bool save();

private:
  MapManager* map;
  QTableWidget* table;
};

#endif
