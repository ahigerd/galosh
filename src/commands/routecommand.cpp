#include "routecommand.h"
#include "mapmanager.h"
#include "explorehistory.h"

RouteCommand::RouteCommand(MapManager* map, ExploreHistory* history)
: QObject(nullptr), TextCommand("ROUTE"), map(map), history(history)
{
  supportedKwargs["-q"] = false;
  supportedKwargs["-g"] = false;
  supportedKwargs["-z"] = false;
}

QString RouteCommand::helpMessage(bool brief) const
{
  if (brief) {
    return "Calculates the path to a specified room";
  }
  // TODO: better docs
  return "/ROUTE [-q] [-g] [-z] <id|waypoint>\n"
    "Calculates a route to the specified room or waypoint and shows the steps to reach it.\n"
    "    -z    Routes to a zone instead of a room or waypoint\n"
    "    -q    Show as a speedwalking path\n"
    "    -g    Immediately run the speedwalking path";
}

void RouteCommand::handleInvoke(const QStringList& args, const KWArgs& kwargs)
{
  int startRoomId;
  if (args.length() > 1) {
    startRoomId = args.first().toInt();
  } else {
    if (!history->currentRoom()) {
      showError("Could not find current room");
      return;
    }
    startRoomId = history->currentRoom()->id;
  }
  map->search()->precompute();
  map->search()->precomputeRoutes();
  QList<int> route;
  QString destName;
  if (kwargs.contains("-z")) {
    const MapZone* zone = map->searchForZone(args.last());
    if (!zone) {
      showError("Could not find destination zone.");
      return;
    }
    if (zone->roomIds.contains(startRoomId)) {
      showError("Already in destination zone.");
      return;
    }
    destName = zone->name;
    route = map->search()->findRoute(startRoomId, destName);
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
      return;
    }
    if (startRoomId == endRoomId) {
      showError("Start room and destination room are the same");
      return;
    }
    route = map->search()->findRoute(startRoomId, endRoomId);
  }
  if (route.isEmpty()) {
    showError(QStringLiteral("Could not find route from %1 to %2").arg(startRoomId).arg(destName));
    return;
  }
  QStringList dirs = map->search()->routeDirections(route);
  if (dirs.isEmpty()) {
    showError(QStringLiteral("Could not find route from %1 to %2").arg(startRoomId).arg(destName));
    return;
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
      emit speedwalk(dirs);
    }
  } else {
    showError(messages.join("\n"));
  }
}
