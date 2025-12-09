#ifndef GALOSH_ITEMDATABASE_H
#define GALOSH_ITEMDATABASE_H

#include <QAbstractListModel>
#include <QMap>
#include <functional>
class QSettings;

class ItemDatabase : public QAbstractListModel
{
Q_OBJECT
public:
  struct EquipSlot {
    QString location;
    QString displayName;
  };
  struct EquipSlotType {
    QString location;
    QString keyword;
    int count = 1;
  };

  ItemDatabase(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;

  QList<int> searchForName(const QStringList& args) const;
  QString itemName(int index) const;
  QString itemStats(const QString& name) const;
  QString itemKeyword(const QString& name) const;
  void setItemKeyword(const QString& name, const QString& keyword);

  void captureEquipment(QObject* context, std::function<void(const QList<EquipSlot>&)> callback);
  EquipSlotType equipmentSlotType(const QString& labelOrKeyword) const;
  void setSlotKeyword(const QString& location, const QString& keyword);
  inline QStringList equipmentSlotOrder() const { return slotOrder; }

public slots:
  void load(const QString& path);
  void processLine(const QString& line);

private slots:
  void abort(QObject* source = nullptr);

private:
  void updateSlotMetadata(const QList<EquipSlot>& equipment);

  QSettings* dbFile;

  struct Capture {
    QString pendingItem;
    QString pendingStats;
    int captureState = 0;
    QList<EquipSlot> equipment;
    std::function<void(const QList<EquipSlot>&)> equipCallback;
  };
  QMap<QObject*, Capture> pendingCaptures;
  QStringList keys;
  QMap<QString, EquipSlotType> slotTypes;
  QMap<QString, QString> slotKeywords;
  QStringList slotOrder;
};

#endif
