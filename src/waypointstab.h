#ifndef GALOSH_WAYPOINTSTAB_H
#define GALOSH_WAYPOINTSTAB_H

#include <QWidget>
#include <QMap>
class QLabel;
class QTableWidget;
class QTableWidgetItem;

class WaypointsTab : public QWidget
{
Q_OBJECT
public:
  WaypointsTab(QWidget* parent = nullptr);

  void load(const QString& profile);
  bool save(const QString& profile);

signals:
  void markDirty();

private slots:
  void addRow();
  void removeRows();
  void onCellChanged(int row, int col);

private:
  QLabel* serverProfile;
  QTableWidget* table;
  QString mapFile;
};

#endif
