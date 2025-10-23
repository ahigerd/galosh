#include "infomodel.h"

InfoModel::InfoModel(QObject* parent)
: QStandardItemModel(0, 2, parent)
{
  itemsByPath[""] = invisibleRootItem();
}

void InfoModel::loadTree(const QVariant& data)
{
  QVariantMap tree = data.toMap();
  if (tree.isEmpty()) {
    return;
  }
  recurseTree("", "", tree);
}

void InfoModel::recurseTree(const QString& path, const QString& key, QVariant value)
{
  value = preprocess(path, key, value);
  QString fullKey = path + "." + key;
  if (fullKey == ".") {
    fullKey = "";
  }
  QStandardItem* parent = itemsByPath[path];
  QStandardItem* item = itemsByPath[fullKey];
  if (!item) {
    itemsByPath[fullKey] = item = new QStandardItem(key);
    item->setData(fullKey, Qt::UserRole);
    parent->appendRow({ item, new QStandardItem });
  }
  if (value.typeId() == QVariant::List) {
    QVariantList list = value.toList();
    int len = list.length();
    for (int i = 0; i < len; i++) {
      recurseTree(fullKey, QString::number(i + 1), list[i]);
    }
    while (item->rowCount() > len) {
      removeChildren(item, item->rowCount() - 1);
    }
  } else if (value.typeId() == QVariant::Map) {
    QVariantMap map = value.toMap();
    for (const QString& subKey : map.keys()) {
      recurseTree(fullKey, subKey, map[subKey]);
    }
    int len = item->rowCount();
    for (int i = 0; i < len; i++) {
      QStandardItem* child = item->child(i, 0);
      if (!child) {
        continue;
      }
      if (!map.contains(child->data(Qt::DisplayRole).toString())) {
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
