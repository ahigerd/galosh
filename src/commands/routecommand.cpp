#include "routecommand.h"
#include "mapmanager.h"
#include "explorehistory.h"

RouteCommand::RouteCommand(MapManager* map, ExploreHistory* history)
: TextCommand("ROUTE"), map(map), history(history)
{
  supportedKwargs["-q"] = false;
  supportedKwargs["-g"] = false;
  supportedKwargs["-z"] = false;
  supportedKwargs["-f"] = false;
}

QString RouteCommand::helpMessage(bool brief) const
{
  if (brief) {
    return "Calculates the path to a specified room";
  }
  // TODO: better docs
  return "/ROUTE [-q] [-g] [-z] [start] <id|waypoint>\n"
    "Calculates a route to the specified room or waypoint and shows the steps to reach it.\n"
    "    -z        Routes to a zone instead of a room or waypoint\n"
    "    -q        Show as a speedwalking path\n"
    "    -g        Immediately run the speedwalking path\n"
    "    -f        With -g, run the speedwalking path in fast mode\n"
    "    start     (Optional) The room ID to start routing from\n"
    "    id        The room ID to route to\n"
    "    waypoint  A predefined waypoint to route to";
}

CommandResult RouteCommand::handleInvoke(const QStringList& args, const KWArgs& kwargs)
{
  if (kwargs.contains("-f") && !kwargs.contains("-g")) {
    showError("Cannot use fast mode without -g.");
    return CommandResult::fail();
  }
  int startRoomId;
  if (args.length() > 1) {
    bool ok = false;
    startRoomId = args.first().toInt(&ok);
    if (!ok) {
      showError("Could not identify start room");
      return CommandResult::fail();
    }
  } else {
    if (!history->currentRoom()) {
      showError("Could not find current room");
      return CommandResult::fail();
    }
    startRoomId = history->currentRoom()->id;
  }
  map->search()->precompute(true);
  map->search()->precomputeRoutes();
  QList<int> route;
  QString destName;
  if (kwargs.contains("-z")) {
    const MapZone* zone = map->searchForZone(args.last());
    if (!zone) {
      showError("Could not find destination zone.");
      return CommandResult::fail();
    }
    if (zone->roomIds.contains(startRoomId)) {
      showError("Already in destination zone.");
      return CommandResult::fail();
    }
    destName = zone->name;
    route = map->search()->findRoute(startRoomId, destName, map->routeAvoidZones());
  } else {
    int endRoomId = args.last().toInt();
    if (endRoomId) {
      destName = QString::number(endRoomId);
    } else {
      destName = args.last();
      endRoomId = map->waypoint(args.last());
    }
    const MapRoom* room = map->room(endRoomId);
    if (!room) {
      showError("Could not find destination room");
      return CommandResult::fail();
    }
    if (startRoomId == endRoomId) {
      showError("Start room and destination room are the same");
      return CommandResult::fail();
    }
    route = map->search()->findRoute(startRoomId, endRoomId, map->routeAvoidZones());
  }
  if (route.isEmpty()) {
    showError(QStringLiteral("Could not find route from %1 to %2").arg(startRoomId).arg(destName));
    return CommandResult::fail();
  }
  QStringList dirs = map->search()->routeDirections(route);
  if (dirs.isEmpty()) {
    showError(QStringLiteral("Could not find route from %1 to %2").arg(startRoomId).arg(destName));
    return CommandResult::fail();
  }
  ExploreHistory path(map);
  path.goTo(startRoomId);
  for (const QString& dir : dirs) {
    path.travel(dir);
  }
  QStringList warnings;
  QStringList messages;
  messages << path.speedwalk(-1, false, &warnings);
  if (kwargs.contains("-q") || kwargs.contains("-g")) {
    if (!warnings.isEmpty()) {
      messages << "";
      messages += warnings;
    }
  } else {
    messages << "";
    messages += path.describe(-1);
  }
  if (warnings.isEmpty()) {
    showMessage(messages.join("\n"));
    if (kwargs.contains("-g")) {
      QStringList walkArgs = QStringList() << " " << "-v" << dirs;
      if (kwargs.contains("-f")) {
        walkArgs << "-f";
      }
      return invokeCommand("SPEEDWALK", walkArgs);
    }
  } else {
    showError(messages.join("\n"));
    return CommandResult::fail();
  }
  return CommandResult::success();
}
