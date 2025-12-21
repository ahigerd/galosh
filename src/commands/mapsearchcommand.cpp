#include "mapsearchcommand.h"
#include "mapmanager.h"

MapSearchCommand::MapSearchCommand(MapManager* map)
: TextCommand("SEARCH"), map(map)
{
  supportedKwargs["-n"] = false;
  supportedKwargs["-z"] = true;
}

QString MapSearchCommand::helpMessage(bool brief) const
{
  if (brief) {
    return "Search for rooms by name and description";
  }
  return
    "Searches for rooms with names or descriptions containing words.\n\n"
    "Words may be regular expressions.\n"
    "Multiple words can be provided. All provided words must be found to match a room.\n"
    "Use '-n' to search only in room names.\n"
    "Use '-z \"zone\"' to search within a single zone.";
}

CommandResult MapSearchCommand::handleInvoke(const QStringList& args, const KWArgs& kwargs)
{
  QString searchZone = kwargs["-z"];
  if (searchZone.startsWith('"') || searchZone.startsWith("'")) {
    searchZone.remove(0, 1);
  }
  if (searchZone.endsWith('"') || searchZone.endsWith("'")) {
    searchZone.chop(1);
  }
  QList<const MapRoom*> results = map->searchForRooms(args, kwargs.contains("-n"), searchZone);
  if (results.isEmpty()) {
    showError("No search results");
    return CommandResult::fail();
  }
  for (const MapRoom* room : results) {
    showMessage(QStringLiteral("[%1] %2 (%3)").arg(room->id).arg(room->name).arg(room->zone));
  }
  return CommandResult::success();
}
