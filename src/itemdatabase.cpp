#include "itemdatabase.h"
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QRegularExpression>
#include <QtDebug>
#include <algorithm>

ItemDatabase::ItemDatabase(QObject* parent)
: QAbstractListModel(parent), dbFile(nullptr), captureState(0)
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
  dbFile->beginGroup("Items");
  keys = dbFile->childKeys();
  std::sort(keys.begin(), keys.end());
}

void ItemDatabase::processLine(const QString& line)
{
  if (captureState == 1) {
    if (line.trimmed().isEmpty()) {
      return;
    }
    captureState = 2;
  } else if (captureState == 2) {
    if (line.trimmed().isEmpty()) {
      captureState = 0;
      for (auto slot : equipment) {
        qDebug() << slot.location << slot.displayName;
      }
      emit capturedEquipment(equipment);
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
      slot.displayName = line.mid(endBracket + 1);
      slot.displayName = slot.displayName.replace("(glowing)", "");
      slot.displayName = slot.displayName.replace("(humming)", "");
      slot.displayName = slot.displayName.replace("(magic)", "");
      slot.displayName = slot.displayName.replace("(good)", "");
      slot.displayName = slot.displayName.replace("(neutral)", "");
      slot.displayName = slot.displayName.replace("(evil)", "");
      slot.displayName = slot.displayName.replace("(invisible)", "");
      slot.displayName = slot.displayName.trimmed();
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

void ItemDatabase::captureEquipment()
{
  captureState = 1;
  equipment.clear();
}

QList<ItemDatabase::EquipSlot> ItemDatabase::lastCapturedEquipment() const
{
  return equipment;
}
