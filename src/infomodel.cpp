#include "infomodel.h"
#include <QtDebug>

InfoModel::InfoModel(QObject* parent)
: QStandardItemModel(0, 2, parent)
{
  itemsByPath[""] = invisibleRootItem();

  setHorizontalHeaderLabels({ "Stat", "Value" });
  setSortRole(Qt::UserRole + 1);
}

QStandardItem* InfoModel::ensureItem(const QString& parentPath, const QString& key)
{
  QString fullKey = parentPath.isEmpty() ? key : (parentPath + "." + key);
  QStandardItem* parent = itemsByPath[parentPath];
  QStandardItem* item = itemsByPath[fullKey];
  if (!item) {
    QString label = labelMap.value(key, key);
    itemsByPath[fullKey] = item = new QStandardItem(label);
    item->setData(fullKey, Qt::UserRole);
    bool isInt = false;
    int intVal = key.toInt(&isInt);
    if (isInt) {
      item->setData(intVal, Qt::UserRole + 1);
    } else {
      item->setData(key, Qt::UserRole + 1);
    }
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    parent->appendRow({ item, new QStandardItem });
    sort(0, Qt::AscendingOrder);
  }
  return item;
}

void InfoModel::gmcpChar(const QString& key, const QVariant& data)
{
  static const QMap<QString, QString> keyMapping = {
    { "CHAR.VITALS", "Vitals" },
    { "CHAR.EFFECTS", "Effects" },
    { "CHAR.STATUS", "Status" },
    { "CHAR.COMBAT", "Combat" },
    { "CHAR.WORTH", "Worth" },
    { "CHAR.SPELLSLOTS", "Spell Slots" },
  };
  static const QSet<QString> ignoreKeys = {
    "CHAR.NAME", // redundant with Char.Status
    "CHAR.SKILLS", // TODO: separate model
    "CHAR.ITEMS.LIST", // TODO: separate model
  };
  if (ignoreKeys.contains(key)) {
    return;
  }
  QString label = keyMapping.value(key);
  if (!label.isEmpty()) {
    ensureItem("", label);
    recurseLegacyTree("", label, data);
  } else if (key == "CHAR.STATUSVARS") {
    labelMap.clear();
    QVariantMap dataMap = data.toMap();
    for (const QString& key : dataMap.keys()) {
      QString value = dataMap[key].toString();
      if (value.isEmpty())
        continue;
      labelMap[key] = value;
    }
    return;
  }
  if (label.isEmpty() || key == "CHAR.EFFECTS") {
    qDebug() << "GMCP:" << key << data;
  }
}

void InfoModel::loadLegacyTree(const QVariant& data)
{
  QVariantMap tree = data.toMap();
  if (tree.isEmpty()) {
    return;
  }
  recurseLegacyTree("", "", tree);
}

void InfoModel::recurseLegacyTree(const QString& path, const QString& key, QVariant value)
{
  value = preprocess(path, key, value);
  QString fullKey = path.isEmpty() ? key : path + "." + key;
  QStandardItem* item = ensureItem(path, key);
  if (value.type() == QVariant::List) {
    QVariantList list = value.toList();
    int len = list.length();
    for (int i = 0; i < len; i++) {
      recurseLegacyTree(fullKey, QString::number(i + 1), list[i]);
    }
    while (item->rowCount() > len) {
      removeChildren(item, item->rowCount() - 1);
    }
  } else if (value.type() == QVariant::Map) {
    QVariantMap map = value.toMap();
    for (const QString& subKey : map.keys()) {
      recurseLegacyTree(fullKey, subKey, map[subKey]);
    }
    int len = item->rowCount();
    for (int i = 0; i < len; i++) {
      QStandardItem* child = item->child(i, 0);
      if (!child) {
        continue;
      }
      if (!map.contains(child->data(Qt::UserRole + 1).toString())) {
        removeChildren(item, i);
        i--;
        len--;
      }
    }
  } else {
    QModelIndex valueIdx = item->index().siblingAtColumn(1);
    setData(valueIdx, value.toString(), Qt::DisplayRole);
  }
}

QVariant InfoModel::preprocess(const QString& path, const QString& key, const QVariant& value)
{
  if (path == "" && (key == "Effects" || key == "Affliction" || key == "Defences")) {
    QVariantMap newValue;
    for (const QVariant& effect : value.toList()) {
      QVariantMap map = effect.toMap();
      QString name = map["name"].toString();
      if (map.contains("duration") || map.contains("duration_hours")) {
        int duration;
        if (map.contains("duration_hours")) {
          duration = map["duration_hours"].toInt();
        } else {
          duration = map["duration"].toInt();
        }
        if (duration < 0) {
          newValue[name] = "permanent";
        } else {
          newValue[name] = QStringLiteral("%1 hours").arg(duration);
        }
      } else if (map.contains("desc")) {
        newValue[name] = map["desc"];
      } else {
        newValue[name] = "";
      }
    }
    return newValue;
  }
  // qDebug() << path << key << value;
  return value;
}

void InfoModel::removeChildren(QStandardItem* parent, int row)
{
  if (parent->rowCount() == 0) {
    return;
  }
  QStandardItem* child = parent->takeRow(row).first();
  itemsByPath.remove(child->data(Qt::UserRole).toString());
  if (child->rowCount()) {
    for (int i = child->rowCount() - 1; i >= 0; --i) {
      removeChildren(child, i);
    }
  }
}
