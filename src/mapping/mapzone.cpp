#include "mapzone.h"
#include "mapmanager.h"
#include "algorithms.h"
#include <QtDebug>

static const QMap<QString, QString> dirAbbrev{
  { "NORTH", "N" },
  { "WEST", "W" },
  { "SOUTH", "S" },
  { "EAST", "E" },
  { "NORTHWEST", "NW" },
  { "SOUTHWEST", "SW" },
  { "NORTHEAST", "NE" },
  { "SOUTHEAST", "SE" },
  { "UP", "U" },
  { "DOWN", "D" },
};

static const QMap<QString, QString> reverseDirs{
  { "N", "S" },
  { "S", "N" },
  { "W", "E" },
  { "E", "W" },
  { "NW", "SE" },
  { "SE", "NW" },
  { "NE", "SW" },
  { "SW", "NE" },
  { "IN", "OUT" },
  { "OUT", "IN" },
  { "U", "D" },
  { "D", "U" },
  { "ENTER", "LEAVE" },
  { "LEAVE", "ENTER" },
  { "SOMEWHERE", "SOMEWHERE" },
};

QString MapRoom::normalizeDir(const QString& dir)
{
  QString norm = dir.simplified().toUpper();
  return dirAbbrev.value(norm, norm);
}

QString MapRoom::reverseDir(const QString& dir)
{
  return reverseDirs.value(dir);
}

bool MapRoom::isDir(const QString& dir)
{
  return reverseDirs.contains(normalizeDir(dir));
}

bool MapRoom::hasExitTo(int dest) const
{
  for (const MapExit& exit : exits) {
    if (exit.dest == dest) {
      return true;
    }
  }
  return false;
}

QString MapRoom::findExit(int dest) const
{
  for (auto [ dir, exit ] : cpairs(exits)) {
    if (exit.dest == dest) {
      return dir;
    }
  }
  return QString();
}

QSet<int> MapRoom::exitRooms() const
{
  QSet<int> rooms;
  for (const MapExit& exit : exits) {
    rooms << exit.dest;
  }
  return rooms;
}

MapZone::MapZone(MapManager* map, const QString& name)
: map(map), name(name)
{
  // initializers only
}

void MapZone::addRoom(const MapRoom* room)
{
  roomIds << room->id;
  for (const MapExit& exit : room->exits) {
    const MapRoom* dest = map->room(exit.dest);
    if (dest && dest->zone != name && !exits.value(dest->zone).contains(room->id)) {
      exits[dest->zone] << room->id;
      map->zoneConnections[name] << dest->zone;

      // Check for a reverse link too
      MapZone* otherZone = map->mutableZone(dest->zone);
      otherZone->addRoom(dest);
    }
  }
}
