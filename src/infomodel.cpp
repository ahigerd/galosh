#include "infomodel.h"
#include <QtDebug>

InfoModel::InfoModel(QObject* parent)
: QStandardItemModel(0, 2, parent)
{
  itemsByPath[""] = invisibleRootItem();

  setHorizontalHeaderLabels({ "Stat", "Value" });
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
    item->setData(key, Qt::UserRole + 1);
    parent->appendRow({ item, new QStandardItem });
  }
  return item;
}

void InfoModel::gmcpChar(const QString& key, const QVariantMap& data)
{
  static const QMap<QString, QString> keyMapping = {
    { "CHAR.VITALS", "Vitals" },
    { "CHAR.EFFECTS", "Effects" },
    { "CHAR.STATUS", "Status" },
    { "CHAR.COMBAT", "Combat" },
  };
  QString label = keyMapping.value(key);
  if (!label.isEmpty()) {
    ensureItem("", label);
    recurseLegacyTree("", label, data);
  } else if (key == "CHAR.STATUSVARS") {
    labelMap.clear();
    for (const QString& key : data.keys()) {
      QString value = data[key].toString();
      if (value.isEmpty())
        continue;
      labelMap[key] = value;
    }
  }
  qDebug() << "GMCP:" << key << data;
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
  if (path == "" && key == "Effects") {
    QVariantMap newValue;
    for (const QVariant& effect : value.toList()) {
      QVariantMap map = effect.toMap();
      QString name = map["name"].toString();
      newValue[name] = QStringLiteral("%1 hours").arg(map["duration"].toInt());
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
