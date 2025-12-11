#include "itemdatabase.h"
#include "algorithms.h"
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QRegularExpression>
#include <QtDebug>
#include <algorithm>

ItemParsers ItemParsers::defaultCircleMudParser = {
  QRegularExpression("(?:Object '(.+)',|You closely examine (.+) \\()"),
  QRegularExpression("Item type: (\\w+)"),
  QRegularExpression("Item is worn: (.*)"),
  QRegularExpression("Item is: (.*)"),
  QRegularExpression("Weight: ([.\\d]+)"),
  QRegularExpression("Value: (\\d+)"),
  QRegularExpression("Level: (\\d+)"),
  QRegularExpression("AC-apply is (\\d+)"),
  QRegularExpression("Apply: (?<mod>[+-]\\d+) to (?<stat>[\\w_]+)"),
  "NO FLAGS",
  // this includes FieryMUD types in addition to basic
  // CircleMUD and some predictable variations
  {
    {"attached to belt", "OBELT"},
    {"held", "HOLD"},
    {"hovering", "HOVER"},
    {"used as light", "LIGHT"},
    {"worn about body", "ABOUT"},
    {"worn about waist", "WAIST"},
    {"worn around neck", "NECK"},
    {"worn on neck", "NECK"},
    {"worn around wrist", "WRIST"},
    {"worn on wrist", "WRIST"},
    {"worn on left wrist", "WRIST"},
    {"worn on right wrist", "WRIST"},
    {"worn on ears", "EAR"},
    {"worn on left ear", "EAR"},
    {"worn on right ear", "EAR"},
    {"worn as badge", "BADGE"},
    {"worn as shield", "SHIELD"},
    {"worn on arms", "ARMS"},
    {"worn on body", "BODY"},
    {"worn on eyes", "EYES"},
    {"worn over eyes", "EYES"},
    {"worn on face", "FACE"},
    {"worn on feet", "FEET"},
    {"worn on finger", "FINGER"},
    {"worn on left finger", "FINGER"},
    {"worn on right finger", "FINGER"},
    {"worn on hands", "HANDS"},
    {"worn on head", "HEAD"},
    {"worn on legs", "LEGS"},
  },
};

template <typename T>
bool containsCompare(const T& searched, const QString& sought, ItemQuery::Comparison compare)
{
  if (compare == ItemQuery::Equal) {
    return searched.contains(sought);
  } else if (compare == ItemQuery::NotEqual) {
    return !searched.contains(sought);
  } else if (compare == ItemQuery::Set) {
    return !searched.isEmpty();
  } else if (compare == ItemQuery::NotSet) {
    return searched.isEmpty();
  } else {
    return false;
  }
}

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
    } else {
      slot.keyword = parsers.slotLocations[location];
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

  populateFlagTypes();
}

void ItemDatabase::processLine(const QString& line)
{
  QObject* source = sender();
  if (!source) {
    return;
  }
  if (!pendingCaptures.contains(source)) {
    QRegularExpressionMatch match = parsers.objectName.match(line);
    if (match.hasMatch()) {
      Capture capture;
      capture.pendingItem = match.captured(match.lastCapturedIndex());
      capture.pendingStats = line;
      pendingCaptures[source] = capture;
    }
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
      pendingCaptures.remove(source);
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
      slot.displayName = slot.displayName.replace("(hovering)", "");
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
            pendingCaptures.remove(source);
            return;
          }
        }
        itemName = QStringLiteral("%1 (%2)").arg(pendingItem).arg(++dedupe);
      } while (!isNew);
      saveItem(itemName, pendingStats);
      if (isNew) {
        endInsertRows();
      } else {
        emit dataChanged(index(row, 0), index(row, 0), { Qt::UserRole });
      }
      pendingCaptures.remove(source);
    } else {
      pendingStats += "\n" + line;
    }
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
      auto& slotType = slotTypes[slot.location];
      slotType.location = slot.location;
      if (slotType.keyword.isEmpty()) {
        slotType.keyword = parsers.slotLocations[slot.location];
        if (!slotType.keyword.isEmpty()) {
          slotKeywords[slotType.keyword] = slot.location;
        }
      }
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
      if (ct > 1) {
        dbFile->setValue(slot + "/count", ct);
      }
    }
    dbFile->endGroup();
    dbFile->beginGroup("Items");
  }
}

ItemStats ItemDatabase::parsedItemStats(const QString& name) const
{
  QString stats = itemStats(name);
  if (stats.isEmpty()) {
    return ItemStats();
  }
  return ItemStats::parse(stats, parsers);
}

ItemStats ItemStats::parse(const QString& stats, const ItemParsers& parsers)
{
  ItemStats item;
  item.formatted = stats;
  item.name = parsers.objectName.match(stats).captured(1);
  item.type = parsers.objectType.match(stats).captured(1);

  QString worn = parsers.objectWorn.match(stats).captured(1);
  worn = worn.replace("\t", " ").toUpper().trimmed();
  item.worn = worn.split(' ', Qt::SkipEmptyParts);

  QString flags = parsers.objectFlags.match(stats).captured(1);
  flags = flags.replace("\t", " ").toUpper().trimmed();
  if (parsers.noFlags.isEmpty() || !flags.contains(parsers.noFlags)) {
    item.flags = flags.split(' ', Qt::SkipEmptyParts);
  }

  item.weight = parsers.objectWeight.match(stats).captured(1).toDouble();
  item.value = parsers.objectValue.match(stats).captured(1).toInt();
  item.level = parsers.objectLevel.match(stats).captured(1).toInt();
  item.armor = parsers.objectArmor.match(stats).captured(1).toInt();

  auto matches = parsers.objectApply.globalMatch(stats);
  while (matches.hasNext()) {
    auto match = matches.next();
    QString stat = match.captured("stat").toUpper();
    int mod = match.captured("mod").toInt();
    item.apply[stat] = mod;
  }

  return item;
}

QList<ItemStats> ItemDatabase::searchForItem(const QList<ItemQuery>& queries) const
{
  QList<ItemStats> results;

  for (const QString& name : keys) {
    ItemStats stats = parsedItemStats(name);
    if (stats.name.isEmpty()) {
      // bogus data
      continue;
    }
    bool match = true;
    for (const ItemQuery& query : queries) {
      QStringList listValue;
      QString strValue;
      double numValue = 0;
      bool isStr = false, isList = false;
      bool isSet = false;
      if (query.stat == "name") {
        isStr = true;
        strValue = stats.name.toLower();
      } else if (query.stat == "type") {
        isStr = true;
        strValue = stats.type.toLower();
      } else if (query.stat == "worn") {
        isList = true;
        listValue = stats.worn;
      } else if (query.stat == "flags") {
        isList = true;
        listValue = stats.flags;
      } else if (query.stat == "weight") {
        numValue = stats.weight;
        isSet = numValue != 0;
      } else if (query.stat == "value") {
        numValue = stats.value;
        isSet = numValue != 0;
      } else if (query.stat == "level") {
        numValue = stats.value;
        isSet = numValue != 0;
      } else if (query.stat == "armor") {
        numValue = stats.armor;
        isSet = numValue != 0;
      } else {
        isSet = stats.apply.contains(query.stat);
        numValue = stats.apply.value(query.stat);
      }
      if (isList) {
        match = containsCompare(listValue, query.value.toString().toUpper(), query.compare);
      } else if (isStr) {
        match = containsCompare(strValue, query.value.toString().toLower(), query.compare);
      } else if (query.compare == ItemQuery::Equal) {
        match = numValue == query.value.toDouble();
      } else if (query.compare == ItemQuery::NotEqual) {
        match = numValue != query.value.toDouble();
      } else if (query.compare == ItemQuery::Greater) {
        match = numValue > query.value.toDouble();
      } else if (query.compare == ItemQuery::Less) {
        match = numValue < query.value.toDouble();
      } else if (query.compare == ItemQuery::GreaterEqual) {
        match = numValue >= query.value.toDouble();
      } else if (query.compare == ItemQuery::LessEqual) {
        match = numValue <= query.value.toDouble();
      } else if (query.compare == ItemQuery::Set) {
        match = isSet;
      } else if (query.compare == ItemQuery::NotSet) {
        match = !isSet;
      }
      if (!match) {
        break;
      }
    }
    if (match) {
      results << stats;
    }
  }

  return results;
}

void ItemDatabase::populateFlagTypes()
{
  for (const QString& name : keys) {
    ItemStats stats = parsedItemStats(name);
    populateFlagTypes(stats);
  }
}

void ItemDatabase::saveItem(const QString& name, const QString& statText)
{
  dbFile->setValue(name, statText);
  ItemStats stats = ItemStats::parse(name, parsers);
  populateFlagTypes(stats);
}

void ItemDatabase::populateFlagTypes(const ItemStats& stats)
{
  if (!stats.type.isEmpty()) {
    itemTypes << stats.type;
  }

  for (const QString& worn : stats.worn) {
    wearTypes << worn;
  }
  wearTypes.remove(QString());

  for (const QString& flag : stats.flags) {
    flagTypes << flag;
  }
  flagTypes.remove(QString());

  for (const QString& apply : stats.apply.keys()) {
    applyTypes << apply;
  }
  applyTypes.remove(QString());
}
