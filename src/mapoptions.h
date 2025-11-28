#ifndef GALOSH_MAPOPTIONS_H
#define GALOSH_MAPOPTIONS_H

#include <QDialog>
class MapManager;
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

private:
  MapManager* map;
  QTableWidget* table;
  bool isDirty;
};

#endif
