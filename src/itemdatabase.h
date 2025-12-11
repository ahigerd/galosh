#ifndef GALOSH_ITEMDATABASE_H
#define GALOSH_ITEMDATABASE_H

#include <QAbstractListModel>
#include <QRegularExpression>
#include <QSet>
#include <QMap>
#include <functional>
class QSettings;

struct ItemParsers {
  static ItemParsers defaultCircleMudParser;

  QRegularExpression objectName;
  QRegularExpression objectType;
  QRegularExpression objectWorn;
  QRegularExpression objectFlags;
  QRegularExpression objectWeight;
  QRegularExpression objectValue;
  QRegularExpression objectLevel;
  QRegularExpression objectArmor;
  QRegularExpression objectApply;
  QString noFlags;

  QMap<QString, QString> slotLocations;
};

struct ItemStats
{
  static ItemStats parse(const QString& stats, const ItemParsers& parsers);

  QString formatted;
  QString name;
  QString type;
  QStringList worn;
  QStringList flags;
  double weight = 0;
  int value = 0;
  int level = 0;
  int armor = 0;
  QMap<QString, int> apply;
  // todo: spell effects?
};

struct ItemQuery
{
  enum Comparison {
    Equal,
    NotEqual,
    Set,
    NotSet,
    Greater,
    Less,
    GreaterEqual,
    LessEqual,
  };

  QString stat;
  QVariant value;
  Comparison compare = Equal;
};

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

  ItemParsers parsers = ItemParsers::defaultCircleMudParser;

  int rowCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;

  QList<int> searchForName(const QStringList& args) const;
  QString itemName(int index) const;
  QString itemStats(const QString& name) const;
  ItemStats parsedItemStats(const QString& name) const;
  QString itemKeyword(const QString& name) const;
  void setItemKeyword(const QString& name, const QString& keyword);

  QList<ItemStats> searchForItem(const QList<ItemQuery>& stats) const;
  void populateFlagTypes();
  inline QStringList knownTypes() const { return QStringList(itemTypes.begin(), itemTypes.end()); }
  inline QStringList knownSlots() const { return QStringList(wearTypes.begin(), wearTypes.end()); }
  inline QStringList knownFlags() const { return QStringList(flagTypes.begin(), flagTypes.end()); }
  inline QStringList knownApply() const { return QStringList(applyTypes.begin(), applyTypes.end()); }

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
  void saveItem(const QString& name, const QString& stats);
  void populateFlagTypes(const ItemStats& stats);

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
  QSet<QString> itemTypes, wearTypes, applyTypes, flagTypes;
};

#endif
