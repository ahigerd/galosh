#include "mudletimport.h"
#include <QIODevice>
#include <QHash>
#include <QMap>
#include <QMultiMap>
#if QT_VERSION >= 0x060000
#include <QMultiMapIterator>
#else
#define QMultiMapIterator QMapIterator
#endif
#include <QSet>
#include <QVector3D>
#include <QPointF>
#include <QColor>
#include <QSizeF>
#include <QPixmap>
#include <QFont>
#include <QtDebug>

template <typename... ARGS>
struct Skip {
  Skip(QDataStream&) {}
};

template <typename FIRST, typename... REST>
struct Skip<FIRST, REST...> {
  Skip(QDataStream& ds)
  {
    FIRST dummy;
    ds >> dummy;
    Skip<REST...>{ds};
  }
};

template <int VERSION>
struct DummyLabel {
  void skip(QDataStream& ds)
  {
    Skip<int>{ds};
    if (VERSION < 12) {
      Skip<QPointF, QPointF>{ds};
    } else if (VERSION < 21) {
      Skip<QVector3D>{ds};
      Skip<QPointF>{ds};
    } else {
      // Where does v21 store positions?
    }
    Skip<QSizeF>{ds};
    Skip<QString>{ds};
    Skip<QColor, QColor>{ds};
    Skip<QPixmap>{ds};
    if (VERSION >= 15) {
      Skip<bool, bool>{ds};
    }
  }
};

template <int VERSION>
QDataStream& operator>>(QDataStream& ds, DummyLabel<VERSION>& dummy)
{
  dummy.skip(ds);
  return ds;
}

template <typename FIRST, typename... REST>
void MudletImport::skip(bool condition, const char*)
{
  if (condition) {
    Skip<FIRST, REST...>{ds};
  }
}

MudletImport::MudletImport(MapManager* map, QIODevice* dev)
: map(map)
{
  // TODO: is JSON support relevant?
  ds.setDevice(dev);
  ds.setVersion(QDataStream::Qt_5_12);

  readBinary();
}

bool MudletImport::readBinary()
{
  QMap<int, QString> zones;

  ds >> version;
  if (version < 4 || version > 127) {
    err = "Not a valid, up-to-date Mudlet map";
    return false;
  }

  skip<QMap<int, int>>("colors");

  ds >> zones;

  skip<QMap<int, QColor>>(version >= 5, "custom colors");
  skip<QMap<QString, int>>(version >= 7);
  skip<QMap<QString, QString>>(version >= 17);
  skip<QFont, qreal, bool>(version >= 19);

  if (version >= 14) {
    int ct;
    ds >> ct;
    for (int i = 0; i < ct; i++) {
      skip<int>();
      skip<QSet<int>>(version >= 18);
      skip<QList<int>>(version < 18);
      skip<QList<int>>();
      skip<QMultiMap<int, QPair<int, int>>>("area exits");
      skip<bool, int, int, int, int, int, int, QVector3D>();
      for (int j = (version >= 17 ? 4 : 6); j > 0; --j) {
        skip<QMap<int, int>>();
      }
      skip<QVector3D, bool, int>();
      skip<qreal>(version >= 21);
      skip<QMap<QString, QString>>(version >= 17);
      if (version >= 21) {
        skipLengthPrefixed<DummyLabel<21>>();
      }
    }
  }

  skip<QHash<QString, int>>(version >= 18);
  skip<int>(version >= 12 && version < 18);

  if (version >= 11 && version <= 20) {
    int areaCt = 0;
    ds >> areaCt;
    while (areaCt-- > 0) {
      int ct;
      ds >> ct;
      skip<int>();
      while (ct-- > 0 && !ds.atEnd()) {
        if (version >= 15) {
          skip<DummyLabel<15>>();
        } else if (version < 12) {
          skip<DummyLabel<11>>();
        } else {
          skip<DummyLabel<14>>();
        }
      }
    }
  }

  while (!ds.atEnd()) {
    int roomID, areaID;
    ds >> roomID >> areaID;
    bool isNewRoom = !map->room(roomID);
    MapRoom* room = map->mutableRoom(roomID);
    room->zone = zones[areaID];

    skip<int, int, int>();

    static const QStringList exitDirs({ "N", "NE", "E", "SE", "S", "SW", "W", "NW", "U", "D", "IN", "OUT" });
    for (const QString& dir : exitDirs) {
      int dest;
      ds >> dest;
      if (dest > 0) {
        room->exits[dir].dest = dest;
      }
    }

    skip<int, int>("env, weight");
    skip<float, float, float, float>(version < 8);
    ds >> room->name;
    skip<bool>();

    if (version >= 21) {
      QMap<QString, int> namedExits;
      ds >> namedExits;
      for (const QString& dir : namedExits.keys()) {
        room->exits[dir.toUpper()].dest = namedExits[dir];
      }
    } else if (version >= 6) {
      QMultiMap<int, QString> namedExits;
      ds >> namedExits;
      for (int key : namedExits.keys()) {
        for (QString value : namedExits.values(key)) {
          QString prefix = value.left(1);
          if (prefix == "0" || prefix == "1") {
            value = value.mid(1);
          }
          MapExit& exit = room->exits[value.toUpper()];
          exit.dest = key;
          if (prefix == "0") {
            exit.door = true;
            exit.locked = false;
          } else if (prefix == "1") {
            exit.door = true;
            exit.open = false;
            exit.locked = true;
            exit.lockable = true;
          }
        }
      }
    }

    skip<QString>(version >= 19);
    skip<qint8>(version >= 9 && version < 19);
    skip<QColor>(version >= 21);
    skip<QMap<QString, QString>>(version >= 10);

    skip<QMap<QString, QList<QPointF>>>(version >= 11);
    skip<QMap<QString, bool>>(version >= 11);
    skip<QMap<QString, QColor>>(version >= 20);
    skip<QMap<QString, QList<int>>>(version < 20 && version >= 11);
    skip<QMap<QString, Qt::PenStyle>>(version >= 20);
    skip<QMap<QString, QString>>(version < 20 && version >= 11);
    if (version >= 21) {
      QSet<QString> locks;
      ds >> locks;
      for (const QString& dir : locks) {
        MapExit& exit = room->exits[dir.toUpper()];
        exit.door = true;
        exit.open = false;
        exit.locked = true;
        exit.lockable = true;
      }
    }
    if (version >= 11) {
      QList<int> locks;
      ds >> locks;
      if (!locks.isEmpty()) {
        qDebug() << "TODO: legacy lock format";
      }
    }
    if (version >= 13) {
      QList<int> stubs;
      ds >> stubs;
      if (!stubs.isEmpty()) {
        qDebug() << "TODO: exit stubs";
      }
    }
    skip<QMap<QString, int>>(version >= 16, "exit weights");
    if (version >= 16) {
      QMap<QString, int> doors;
      ds >> doors;
      for (const QString& dir : doors.keys()) {
        int status = doors[dir];
        MapExit& exit = room->exits[dir.toUpper()];
        exit.door = status > 0;
        exit.open = status < 2;
        exit.lockable = status == 3;
        exit.locked = exit.lockable;
      }
    }

    if (isNewRoom && !room->name.isEmpty()) {
      qDebug() << "importing" << room->name << room->id;
      map->saveRoom(room);
    } else {
      // What should be updated, what should be ignored?
      qDebug() << "skipping" << room->name << room->id;
    }
  }
  return true;
}
