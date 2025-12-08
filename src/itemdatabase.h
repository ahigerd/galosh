#ifndef GALOSH_ITEMDATABASE_H
#define GALOSH_ITEMDATABASE_H

#include <QAbstractListModel>
#include <QMap>
class QSettings;

class ItemDatabase : public QAbstractListModel
{
Q_OBJECT
public:
  struct EquipSlot {
    QString location;
    QString displayName;
  };

  ItemDatabase(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;

  QList<int> searchForName(const QStringList& args) const;
  QString itemName(int index) const;
  QString itemStats(const QString& name) const;
  QString itemKeyword(const QString& name) const;
  void setItemKeyword(const QString& name, const QString& keyword);

  void captureEquipment();
  QList<EquipSlot> lastCapturedEquipment() const;

signals:
  void capturedEquipment(const QList<EquipSlot>& equipment);

public slots:
  void load(const QString& path);
  void processLine(const QString& line);

private:
  QSettings* dbFile;
  QString pendingItem;
  QString pendingStats;
  QStringList keys;

  int captureState;
  QList<EquipSlot> equipment;
};

#endif
