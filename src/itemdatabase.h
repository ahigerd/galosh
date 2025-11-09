#ifndef ITEMDATABASE_H
#define ITEMDATABASE_H

#include <QAbstractListModel>
#include <QMap>
class QSettings;

class ItemDatabase : public QAbstractListModel
{
Q_OBJECT
public:
  ItemDatabase(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;

  QList<int> searchForName(const QStringList& args) const;
  QString itemName(int index) const;
  QString itemStats(const QString& name) const;

public slots:
  void loadProfile(const QString& profile);
  void processLine(const QString& line);

private:
  QSettings* dbFile;
  QString pendingItem;
  QString pendingStats;
  QStringList keys;
};

#endif
