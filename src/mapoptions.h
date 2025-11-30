#ifndef GALOSH_MAPOPTIONS_H
#define GALOSH_MAPOPTIONS_H

#include <QDialog>
class MapManager;
class QListWidget;
class QTableWidget;
class QTableWidgetItem;

class MapOptions : public QDialog
{
Q_OBJECT
public:
  MapOptions(MapManager* map, QWidget* parent = nullptr);

  void done(int r) override;

public slots:
  bool save();

private slots:
  void addRow();
  void removeRows();
  void onCellChanged(int row, int col);
  void onItemDoubleClicked(QTableWidgetItem* item);

  void addAvoid();
  void removeAvoids();

private:
  QWidget* makeColorTab(QWidget* parent);
  QWidget* makeRoutingTab(QWidget* parent);

  MapManager* map;
  QTableWidget* table;
  QListWidget* avoid;
  bool isDirty;
};

#endif
