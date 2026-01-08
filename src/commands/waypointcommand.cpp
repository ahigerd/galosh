#include "waypointcommand.h"
#include "mapmanager.h"
#include "mapsearch.h"
#include "explorehistory.h"

WaypointCommand::WaypointCommand(MapManager* map, ExploreHistory* history)
: TextCommand("WAYPOINT"), map(map), history(history)
{
  supportedKwargs["-a"] = true;
  supportedKwargs["-d"] = true;
  supportedKwargs["-g"] = true;
  supportedKwargs["-f"] = false;
  addKeyword("WAY");
}

QString WaypointCommand::helpMessage(bool brief) const
{
  if (brief) {
    return "Adds, views, removes, or routes to waypoints";
  }
  return "Manages the list of waypoints.\n"
    "/WAYPOINT                 Lists known waypoints\n"
    "/WAYPOINT <name>          Shows information about the named waypoint\n"
    "/WAYPOINT -a <name>       Creates or updates a waypoint in the current room\n"
    "/WAYPOINT -a <name> <id>  Creates or updates a waypoint in the specified room\n"
    "/WAYPOINT -d <name>       Deletes the named waypoint\n"
    "/WAYPOINT -g <name>       Speedwalks to the named waypoint\n"
    "             -f           Runs speedwalk in fast mode";
}

CommandResult WaypointCommand::handleInvoke(const QStringList& args, const KWArgs& kwargs)
{
  if (kwargs.contains("-f") && !kwargs.contains("-g")) {
    showError("Cannot use fast mode without -g.");
    return CommandResult::fail();
  }
  if (!kwargs.isEmpty() && !args.isEmpty() && !kwargs.contains("-a")) {
    showError("Unexpected parameter: " + args.join(" "));
    return CommandResult::fail();
  } else if (kwargs.contains("-d")) {
    return handleDelete(kwargs.value("-d"));
  } else if (kwargs.contains("-g")) {
    return handleRoute(kwargs.value("-g"), true, kwargs.contains("-f"));
  } else if (kwargs.contains("-a")) {
    if (args.length() == 1) {
      bool ok;
      int roomId = args[0].toInt(&ok);
      return handleAdd(kwargs.value("-a"), ok ? roomId : -1);
    } else if (args.length() == 0) {
      return handleAdd(kwargs.value("-a"), history->currentRoom() ? history->currentRoom()->id : -1);
    }
    return CommandResult::fail();
  } else if (args.length() == 1) {
    return handleRoute(args[0], false, false);
  } else {
    QStringList waypoints = map->waypoints();
    if (waypoints.isEmpty()) {
      showMessage("No waypoints defined.");
      return CommandResult::fail();
    }
    showMessage("Waypoints:");
    int width = 0;
    for (const QString& waypoint : waypoints) {
      if (width < waypoint.length()) {
        width = waypoint.length();
      }
    }
    for (const QString& waypoint : map->waypoints()) {
      int roomId = map->waypoint(waypoint);
      const MapRoom* room = map->room(roomId);
      QString name("[invalid]");
      if (room) {
        if (room->zone.isEmpty()) {
          name = room->name;
        } else {
          name = QStringLiteral("%1: %2").arg(room->zone).arg(room->name);
        }
      }
      showMessage(QStringLiteral("%1\t%2").arg(waypoint, -width).arg(name));
    }
  }
  return CommandResult::success();
}

CommandResult WaypointCommand::handleAdd(const QString& name, int roomId)
{
  if (roomId < 0) {
    showError("Could not find target room for waypoint.");
    return CommandResult::fail();
  }
  if (map->setWaypoint(name, roomId)) {
    showMessage("Waypoint \"" + name + "\" created.");
    return CommandResult::success();
  } else {
    showError("Could not create waypoint.");
    return CommandResult::fail();
  }
}

CommandResult WaypointCommand::handleDelete(const QString& name)
{
  if (map->removeWaypoint(name)) {
    showMessage("Waypoint \"" + name + "\" removed.");
    return CommandResult::success();
  } else {
    showError("Could not remove waypoint.");
    return CommandResult::fail();
  }
}

CommandResult WaypointCommand::handleRoute(const QString& name, bool run, bool fast)
{
  int startRoomId = history->currentRoom() ? history->currentRoom()->id : -1;
  if (startRoomId < 0) {
    showError("Could not find current room.");
    return CommandResult::fail();
  }
  QString realName;
  int endRoomId = map->waypoint(name, &realName);
  const MapRoom* room = map->room(endRoomId);
  if (endRoomId < 0 || !room) {
    showError("Could not find waypoint.");
    return CommandResult::fail();
  }
  QString zoneName = room->zone.isEmpty() ? "" : room->zone + ": ";
  showMessage(QStringLiteral("Waypoint \"%1\": [%2] %3%4").arg(realName).arg(endRoomId).arg(zoneName).arg(room->name));
  if (startRoomId == endRoomId) {
    if (run) {
      showError("Already at destination.");
    } else {
      showMessage("Currently at waypoint.");
    }
    return CommandResult::success();
  }
  map->search()->precompute(true);
  map->search()->precomputeRoutes();
  QList<int> route = map->search()->findRoute(startRoomId, endRoomId, map->routeAvoidZones());
  QStringList dirs = route.isEmpty() ? QStringList() : map->search()->routeDirections(route);
  if (dirs.isEmpty()) {
    if (run) {
      showError(QStringLiteral("Could not find route from %1 to %2.").arg(startRoomId).arg(endRoomId));
    } else {
      showMessage(QStringLiteral("Could not find route to waypoint from current room."));
    }
    return CommandResult::fail();
  }
  if (run) {
    dirs << " " << "-v";
    if (fast) {
      dirs << "-f";
    }
    return invokeCommand("\x01SPEEDWALK", dirs);
  } else {
    ExploreHistory path(map);
    path.goTo(startRoomId);
    for (const QString& dir : dirs) {
      path.travel(dir);
    }
    showMessage("Route: " + path.speedwalk(-1));
  }
  return CommandResult::success();
}
