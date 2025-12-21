#include "zonecommand.h"
#include "mapmanager.h"

ZoneCommand::ZoneCommand(MapManager* map)
: TextCommand("ZONES"), map(map)
{
  addKeyword("ZONE");
}

QString ZoneCommand::helpMessage(bool brief) const
{
  if (brief) {
    return "List known zones, or show information about a zone";
  }
  return
    "If used without parameters, lists all known zones.\n"
    "If used with a zone name, shows information about that zone.";
}

CommandResult ZoneCommand::handleInvoke(const QStringList& args, const KWArgs&)
{
  if (args.isEmpty()) {
    for (const QString& zone : map->zoneNames()) {
      showMessage(zone);
    }
    return CommandResult::success();
  }
  QString zoneName = args.join(' ');
  const MapZone* zone = map->searchForZone(zoneName);
  if (!zone) {
    showError("Unknown zone: " + zoneName);
    return CommandResult::fail();
  }
  showMessage(zone->name);
  int minRoom = 0, maxRoom = 0, numRooms = zone->roomIds.size();
  for (int roomId : zone->roomIds) {
    if (!minRoom || roomId < minRoom) {
      minRoom = roomId;
    }
    if (!maxRoom || roomId > maxRoom) {
      maxRoom = roomId;
    }
  }
  showMessage(QStringLiteral("%1 mapped rooms (%2 - %3)").arg(numRooms).arg(minRoom).arg(maxRoom));
  showMessage("");
  showMessage("Connected zones:");
  for (const QString& name : zone->exits.keys()) {
    if (name.isEmpty() || name == "-") {
      // placeholders for non-zones
      continue;
    }
    showMessage(name);
  }
  return CommandResult::success();
}
