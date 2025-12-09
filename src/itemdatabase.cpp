#include "itemdatabase.h"
#include "algorithms.h"
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QRegularExpression>
#include <QtDebug>
#include <algorithm>

ItemDatabase::ItemDatabase(QObject* parent)
: QAbstractListModel(parent), dbFile(nullptr)
{
  // initializers only
}

int ItemDatabase::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return keys.count();
}

QVariant ItemDatabase::data(const QModelIndex& index, int role) const
{
  if (index.parent().isValid() || index.column() != 0 || index.row() >= keys.count()) {
    return QVariant();
  }
  if (role == Qt::DisplayRole) {
    return keys[index.row()];
  } else if (role == Qt::UserRole) {
    return dbFile->value(keys[index.row()]);
  } else {
    return QVariant();
  }
}

void ItemDatabase::load(const QString& path)
{
  dbFile = new QSettings(path, QSettings::IniFormat);
  dbFile->beginGroup("Equipment");
  QMap<int, QString> order;
  for (const QString& location : dbFile->childGroups()) {
    dbFile->beginGroup(location);
    EquipSlotType slot;
    slot.location = location;
    slot.keyword = dbFile->value("keyword").toString();
    slot.count = dbFile->value("count", 1).toInt();
    slotTypes[location] = slot;
    if (!slot.keyword.isEmpty()) {
      slotKeywords[slot.keyword] = location;
    }
    order[dbFile->value("order").toInt()] = location;
    dbFile->endGroup();
  }
  slotOrder = order.values();
  dbFile->endGroup();

  dbFile->beginGroup("Items");
  keys = dbFile->childKeys();
  std::sort(keys.begin(), keys.end());
}

void ItemDatabase::processLine(const QString& line)
{
  QObject* source = sender();
  if (!pendingCaptures.contains(source)) {
    return;
  }

  auto& [pendingItem, pendingStats, captureState, equipment, equipCallback] = pendingCaptures[source];

  if (captureState == 1) {
    if (line.trimmed().isEmpty()) {
      return;
    }
    captureState = 2;
  } else if (captureState == 2) {
    if (line.trimmed().isEmpty()) {
      captureState = 0;
      updateSlotMetadata(equipment);
      equipCallback(equipment);
      return;
    }
    int bracket = line.indexOf('<');
    if (bracket >= 0) {
      int endBracket = line.indexOf('>', bracket);
      if (endBracket < 0) {
        return;
      }
      EquipSlot slot;
      slot.location = line.mid(bracket + 1, endBracket - bracket - 1);
      slot.displayName = line.mid(endBracket + 1).split("[").first();
      slot.displayName = slot.displayName.replace("(glowing)", "");
      slot.displayName = slot.displayName.replace("(humming)", "");
      slot.displayName = slot.displayName.replace("(magic)", "");
      slot.displayName = slot.displayName.replace("(good)", "");
      slot.displayName = slot.displayName.replace("(neutral)", "");
      slot.displayName = slot.displayName.replace("(evil)", "");
      slot.displayName = slot.displayName.replace("(invisible)", "");
      slot.displayName = slot.displayName.trimmed();
      if (slot.displayName == "Nothing.") {
        slot.displayName.clear();
      }
      equipment << slot;
    }
  }
  if (!dbFile) {
    return;
  }
  if (!pendingItem.isEmpty()) {
    if (line.trimmed().isEmpty()) {
      bool isNew;
      int row;
      int dedupe = 1;
      QString itemName = pendingItem;
      do {
        row = keys.indexOf(itemName);
        isNew = row < 0;
        if (isNew) {
          QStringList newKeys = keys;
          newKeys << itemName;
          std::sort(newKeys.begin(), newKeys.end());
          row = keys.indexOf(itemName);
          beginInsertRows(QModelIndex(), row, row);
          keys = newKeys;
        } else {
          QString existing = dbFile->value(itemName).toString();
          if (pendingStats == existing) {
            // no change
            pendingItem.clear();
            pendingStats.clear();
            return;
          }
        }
        itemName = QStringLiteral("%1 (%2)").arg(pendingItem).arg(++dedupe);
      } while (!isNew);
      dbFile->setValue(itemName, pendingStats);
      if (isNew) {
        endInsertRows();
      } else {
        emit dataChanged(index(row, 0), index(row, 0), { Qt::UserRole });
      }
      pendingItem.clear();
      pendingStats.clear();
    } else {
      pendingStats += "\n" + line;
    }
  } else if (line.startsWith("Object ")) {
    int start = line.indexOf("'");
    if (start < 0) {
      return;
    }
    int end = line.lastIndexOf("'");
    if (end <= start) {
      return;
    }
    pendingItem = line.mid(start + 1, end - start - 1);
    pendingStats = line;
  }
}

QList<int> ItemDatabase::searchForName(const QStringList& args) const
{
  if (args.isEmpty()) {
    return {};
  }
  QList<int> filtered;
  filtered.reserve(keys.count());
  for (int i = 0; i < keys.count(); i++) {
    filtered << (i + 1);
  }
  QRegularExpression re(args.first(), QRegularExpression::CaseInsensitiveOption);
  QString matchZone;
  for (QString arg : args) {
    re.setPattern(arg.replace("-", ".*"));
    QList<int> nextRound;
    for (int item : filtered) {
      if (re.match(keys[item - 1]).hasMatch()) {
        nextRound << item;
      }
    }
    filtered = nextRound;
  }
  return filtered;
}

QString ItemDatabase::itemName(int index) const
{
  if (index <= 0 || index > keys.count()) {
    return QString();
  }
  return keys[index - 1];
}

QString ItemDatabase::itemStats(const QString& name) const
{
  if (!dbFile) {
    return QString();
  }
  return dbFile->value(name).toString();
}

QString ItemDatabase::itemKeyword(const QString& name) const
{
  if (!dbFile) {
    return QString();
  }
  return dbFile->value(name + "/keyword").toString();
}

void ItemDatabase::setItemKeyword(const QString& name, const QString& keyword)
{
  if (!dbFile) {
    qWarning() << "ItemDatabase::setItemKeyword called with no database file";
    return;
  }
  if (keyword.isEmpty()) {
    dbFile->remove(name + "/keyword");
  } else {
    dbFile->setValue(name + "/keyword", keyword);
  }
}

void ItemDatabase::captureEquipment(QObject* context, std::function<void(const QList<EquipSlot>&)> callback)
{
  QObject::connect(context, SIGNAL(destroyed(QObject*)), this, SLOT(abort(QObject*)));
  pendingCaptures[context].equipCallback = callback;
  pendingCaptures[context].captureState = 1;
  pendingCaptures[context].equipment.clear();
}

void ItemDatabase::abort(QObject* source)
{
  pendingCaptures.remove(source);
}

ItemDatabase::EquipSlotType ItemDatabase::equipmentSlotType(const QString& labelOrKeyword) const
{
  if (slotKeywords.contains(labelOrKeyword)) {
    return slotTypes[slotKeywords[labelOrKeyword]];
  }
  return slotTypes[labelOrKeyword];
}

void ItemDatabase::setSlotKeyword(const QString& location, const QString& keyword)
{
  QString oldKeyword = slotTypes[location].keyword;
  if (!oldKeyword.isEmpty() && slotKeywords.contains(oldKeyword)) {
    slotKeywords.remove(oldKeyword);
  }
  slotKeywords[keyword] = location;
  slotTypes[location].keyword = keyword;

  if (dbFile) {
    dbFile->endGroup();
    dbFile->beginGroup("Equipment");
    dbFile->setValue(QStringLiteral("Equipment/%1/keyword").arg(location), keyword);
    dbFile->endGroup();
    dbFile->beginGroup("Items");
  }
}

void ItemDatabase::updateSlotMetadata(const QList<EquipSlot>& equipment)
{
  if (equipment.isEmpty()) {
    return;
  }

  QMap<QString, int> slotCount;
  QStringList newSlotOrder;
  bool updatedCount = false;
  for (const auto& slot : equipment) {
    int ct = ++slotCount[slot.location];
    if (ct == 1) {
      newSlotOrder << slot.location;
    }
    if (slotTypes[slot.location].count < ct) {
      updatedCount = updatedCount || (ct > 1);
      slotTypes[slot.location].count = ct;
    }
  }
  if (!slotOrder.isEmpty()) {
    int last = slotOrder.length() - 1;
    QString prev = slotOrder[0];
    for (int i = 1; i <= last; i++) {
      QString slot = slotOrder[i];
      if (!newSlotOrder.contains(slot)) {
        int after = newSlotOrder.indexOf(prev);
        if (after >= 0) {
          newSlotOrder.insert(after + 1, slot);
        }
      }
      prev = slot;
    }
    QString next = slotOrder[last];
    if (!newSlotOrder.contains(next)) {
      newSlotOrder << next;
    }
    for (int i = last - 1; i >= 0; --i) {
      QString slot = slotOrder[i];
      if (!newSlotOrder.contains(slot)) {
        int before = newSlotOrder.indexOf(next);
        if (before >= 0) {
          newSlotOrder.insert(before, slot);
        }
      }
      next = slot;
    }
  }
  if (slotOrder == newSlotOrder && !updatedCount) {
    return;
  }
  slotOrder = newSlotOrder;

  if (dbFile) {
    dbFile->endGroup();
    dbFile->beginGroup("Equipment");
    for (auto [i, slot] : enumerate(slotOrder)) {
      dbFile->setValue(slot + "/order", i + 1);
      int ct = slotTypes[slot].count;
      qDebug() << slot << ct;
      if (ct > 1) {
        dbFile->setValue(slot + "/count", ct);
      }
    }
    dbFile->endGroup();
    dbFile->beginGroup("Items");
  }
}
