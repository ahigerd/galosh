#include "itemdatabase.h"
#include "algorithms.h"
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QRegularExpression>
#include <QtDebug>
#include <algorithm>

static QRegularExpression standardName("Object '(.*)'");

ItemParsers ItemParsers::defaultCircleMudParser = {
  QRegularExpression("(?:Object '(.+)', ?|You closely examine (.+) \\(.*$)"),
  QRegularExpression("Item type: (\\w+)"),
  QRegularExpression("Item is (?:worn|wielded): (.*)"),
  QRegularExpression("Item is: (.*)"),
  QRegularExpression("Weight: ([.\\d]+)"),
  QRegularExpression("Value: (\\d+)"),
  QRegularExpression("Level: (\\d+)"),
  QRegularExpression("AC-apply is (\\d+)"),
  QRegularExpression("Apply: (?<mod>[+-]\\d+) to (?<stat>[\\w_]+)"),
  QRegularExpression("Damage Dice is '(?<range>[D\\d]+)' for an average per-round damage of (?<average>[.\\d]+)."),
  QRegularExpression("Damage Type is (\\w+)."),
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
    {"worn in ears", "EAR"},
    {"worn in left ear", "EAR"},
    {"worn in right ear", "EAR"},
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
    {"wielded", "ONE-HANDED"},
    {"wielded secondary", "ONE-HANDED"},
    {"wielded two-handed", "TWO-HANDED"},
  },
};

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
  dbFile->beginGroup("Parsers");
  parsers = ItemParsers::defaultCircleMudParser;
  if (dbFile->contains("name")) {
    parsers.objectName.setPattern(dbFile->value("name").toString());
  }
  if (dbFile->contains("type")) {
    parsers.objectType.setPattern(dbFile->value("type").toString());
  }
  if (dbFile->contains("worn")) {
    parsers.objectWorn.setPattern(dbFile->value("worn").toString());
  }
  if (dbFile->contains("flags")) {
    parsers.objectFlags.setPattern(dbFile->value("flags").toString());
  }
  if (dbFile->contains("weight")) {
    parsers.objectWeight.setPattern(dbFile->value("weight").toString());
  }
  if (dbFile->contains("value")) {
    parsers.objectValue.setPattern(dbFile->value("value").toString());
  }
  if (dbFile->contains("level")) {
    parsers.objectLevel.setPattern(dbFile->value("level").toString());
  }
  if (dbFile->contains("armor")) {
    parsers.objectArmor.setPattern(dbFile->value("armor").toString());
  }
  if (dbFile->contains("apply")) {
    parsers.objectApply.setPattern(dbFile->value("apply").toString());
  }
  if (dbFile->contains("damage")) {
    parsers.objectDamage.setPattern(dbFile->value("damage").toString());
  }
  if (dbFile->contains("damageType")) {
    parsers.objectDamageType.setPattern(dbFile->value("damageType").toString());
  }
  if (dbFile->contains("noFlags")) {
    parsers.noFlags = dbFile->value("noFlags").toString();
  }
  dbFile->beginGroup("slot");
  QMap<QString, QString> loadedSlotLocations;
  for (const QString& key : dbFile->childKeys()) {
    loadedSlotLocations[key] = dbFile->value(key).toString();
  }
  if (!loadedSlotLocations.isEmpty()) {
    parsers.slotLocations = loadedSlotLocations;
  }
  dbFile->endGroup();
  dbFile->endGroup();

  dbFile->beginGroup("Equipment");
  QMap<int, QString> order;
  for (const QString& location : dbFile->childGroups()) {
    dbFile->beginGroup(location);
    EquipSlotType slot;
    slot.location = location;
    slot.keyword = dbFile->value("keyword").toString();
    slot.count = dbFile->value("count", 1).toInt();
    if (!slot.keyword.isEmpty()) {
      slotKeywords[slot.keyword] = location;
    } else {
      slot.keyword = parsers.slotLocations[location];
      slotKeywords[slot.keyword] = location;
    }
    slotTypes[location] = slot;
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
      capture.pendingStats = QStringLiteral("Object '%1'").arg(capture.pendingItem);
      QString remaining = QString(line).replace(match.captured(0), "").trimmed();
      if (!remaining.isEmpty()) {
        capture.pendingStats += "\n" + remaining;
      }
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
      pendingStats = pendingStats.trimmed();
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
          QString firstLine = existing.section('\n', 0, 0);
          QRegularExpressionMatch match = parsers.objectName.match(firstLine);
          if (!match.hasMatch()) {
            match = standardName.match(firstLine);
          }
          if (match.hasMatch()) {
            QString remaining = QString(existing).replace(match.captured(0), "").trimmed();
            QString normalized = QStringLiteral("Object '%1'").arg(match.captured(match.lastCapturedIndex()));
            if (!remaining.isEmpty()) {
              normalized += "\n" + remaining;
            }
            if (pendingStats == normalized) {
              // update legacy data structure
              break;
            }
          }
          itemName = QStringLiteral("%1 (%2)").arg(pendingItem).arg(++dedupe);
        }
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
  for (auto [location, slot] : cpairs(parsers.slotLocations)) {
    slotTypes[location].location = location;
    slotTypes[location].keyword = slot;
  }
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
  ItemStats item = ItemStats::parse(stats, parsers);
  if (item.name.isEmpty()) {
    item.name = name;
  }
  return item;
}

static QString getMatch(const QRegularExpression& re, const QString& subject)
{
  auto match = re.match(subject);
  if (!match.hasMatch()) {
    return QString();
  }
  return match.captured(match.lastCapturedIndex());
}

ItemStats ItemStats::parse(const QString& stats, const ItemParsers& parsers)
{
  ItemStats item;
  item.formatted = stats;
  item.name = getMatch(parsers.objectName, stats);
  if (item.name.isEmpty()) {
    item.name = getMatch(standardName, stats);
  }
  item.type = getMatch(parsers.objectType, stats);

  auto matches = parsers.objectWorn.globalMatch(stats);
  while (matches.hasNext()) {
    auto match = matches.next();
    QString worn = match.captured(match.lastCapturedIndex());
    worn = worn.replace("\t", " ").toUpper().trimmed();
    item.worn << worn.split(' ', Qt::SkipEmptyParts);
  }

  QString flags = getMatch(parsers.objectFlags, stats);
  flags = flags.replace("\t", " ").toUpper().trimmed();
  if (parsers.noFlags.isEmpty() || !flags.contains(parsers.noFlags)) {
    item.flags = flags.split(' ', Qt::SkipEmptyParts);
  }

  item.weight = getMatch(parsers.objectWeight, stats).toDouble();
  item.value = getMatch(parsers.objectValue, stats).toInt();
  item.level = getMatch(parsers.objectLevel, stats).toInt();
  item.armor = getMatch(parsers.objectArmor, stats).toInt();
  item.damageType = getMatch(parsers.objectDamageType, stats);

  auto match = parsers.objectDamage.match(stats);
  item.damage = match.captured("range");
  item.averageDamage = match.captured("average").toDouble();

  matches = parsers.objectApply.globalMatch(stats);
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
        numValue = stats.level;
        isSet = numValue != 0;
      } else if (query.stat == "armor") {
        numValue = stats.armor;
        isSet = numValue != 0;
      } else if (query.stat == "damage") {
        numValue = stats.averageDamage;
        isSet = numValue != 0;
      } else {
        isSet = stats.apply.contains(query.stat);
        numValue = stats.apply.value(query.stat);
      }
      bool matchAll = query.compare == ItemQuery::Equal || query.compare == ItemQuery::All || query.compare == ItemQuery::Set;
      // bool matchAny = query.compare == ItemQuery::Any; // implicit
      bool matchNone = query.compare == ItemQuery::NotEqual || query.compare == ItemQuery::None || query.compare == ItemQuery::NotSet;
      if (isList) {
        bool ok = matchNone;
        for (const QString& v : query.value.toStringList()) {
          if (listValue.contains(v.toUpper())) {
            if (matchNone) {
              ok = false;
              break;
            }
            ok = true;
          } else if (matchAll) {
            ok = false;
            break;
          }
        }
        match = ok;
      } else if (isStr) {
        bool ok = matchNone;
        for (const QString& v : query.value.toStringList()) {
          if (strValue.contains(v.toLower())) {
            if (matchNone) {
              ok = false;
              break;
            }
            ok = true;
          } else if (matchAll) {
            ok = false;
            break;
          }
        }
        match = ok;
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
        qDebug() << name << query.stat << numValue << "<=" << query.value.toDouble() << match;
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
