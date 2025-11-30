#include "waypointcommand.h"
#include "mapmanager.h"
#include "mapsearch.h"
#include "explorehistory.h"

WaypointCommand::WaypointCommand(MapManager* map, ExploreHistory* history)
: QObject(nullptr), TextCommand("WAYPOINT"), map(map), history(history)
{
  supportedKwargs["-a"] = true;
  supportedKwargs["-d"] = true;
  supportedKwargs["-g"] = true;
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
    "/WAYPOINT -g <name>       Speedwalks to the named waypoint";
}

void WaypointCommand::handleInvoke(const QStringList& args, const KWArgs& kwargs)
{
  if (!kwargs.isEmpty() && !args.isEmpty() && !kwargs.contains("-a")) {
    showError("Unexpected parameter: " + args.join(" "));
  } else if (kwargs.contains("-d")) {
    handleDelete(kwargs.value("-d"));
  } else if (kwargs.contains("-g")) {
    handleRoute(kwargs.value("-g"), true);
  } else if (kwargs.contains("-a")) {
    if (args.length() == 1) {
      bool ok;
      int roomId = args[0].toInt(&ok);
      handleAdd(kwargs.value("-a"), ok ? roomId : -1);
    } else if (args.length() == 0) {
      handleAdd(kwargs.value("-a"), history->currentRoom() ? history->currentRoom()->id : -1);
    }
  } else if (args.length() == 1) {
    handleRoute(args[0], false);
  } else {
    QStringList waypoints = map->waypoints();
    if (waypoints.isEmpty()) {
      showMessage("No waypoints defined.");
      return;
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
}

void WaypointCommand::handleAdd(const QString& name, int roomId)
{
  if (roomId < 0) {
    showError("Could not find room for waypoint.");
    return;
  }
  if (map->setWaypoint(name, roomId)) {
    showMessage("Waypoint \"" + name + "\" created.");
  } else {
    showError("Could not create waypoint.");
  }
}

void WaypointCommand::handleDelete(const QString& name)
{
  if (map->removeWaypoint(name)) {
    showMessage("Waypoint \"" + name + "\" removed.");
  } else {
    showError("Could not remove waypoint.");
  }
}

void WaypointCommand::handleRoute(const QString& name, bool run)
{
  int startRoomId = history->currentRoom() ? history->currentRoom()->id : -1;
  if (startRoomId < 0) {
    showError("Could not find current room.");
    return;
  }
  QString realName;
  int endRoomId = map->waypoint(name, &realName);
  const MapRoom* room = map->room(endRoomId);
  if (endRoomId < 0 || !room) {
    showError("Could not find waypoint.");
    return;
  }
  QString zoneName = room->zone.isEmpty() ? "" : room->zone + ": ";
  showMessage(QStringLiteral("Waypoint \"%1\": [%2] %3%4").arg(realName).arg(endRoomId).arg(zoneName).arg(room->name));
  if (startRoomId == endRoomId) {
    if (run) {
      showError("Already at destination.");
    } else {
      showMessage("Currently at waypoint.");
    }
    return;
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
    return;
  }
  if (run) {
    emit speedwalk(dirs);
  } else {
    ExploreHistory path(map);
    path.goTo(startRoomId);
    for (const QString& dir : dirs) {
      path.travel(dir);
    }
    showMessage("Route: " + path.speedwalk(-1));
  }
}
