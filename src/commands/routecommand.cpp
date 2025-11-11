#include "routecommand.h"
#include "mapmanager.h"
#include "explorehistory.h"

RouteCommand::RouteCommand(MapManager* map, ExploreHistory* history)
: TextCommand("ROUTE"), map(map), history(history)
{
}

QString RouteCommand::helpMessage(bool brief) const
{
  if (brief) {
    return "List known zones, or show information about a zone";
  }
  return
    "If used without parameters, lists all known zones.\n"
    "If used with a zone name, shows information about that zone.";
}

void RouteCommand::handleInvoke(const QStringList& args, const KWArgs&)
{
  if (!history->currentRoom()) {
    showError("Could not find current room");
    return;
  }
  int endRoomId = args.first().toInt();
  const MapRoom* room = map->room(endRoomId);
  if (!room) {
    showError("Could not find destination room");
    return;
  }
  if (room == history->currentRoom()) {
    showError("Start room and destination room are the same");
    return;
  }
  int startRoomId = history->currentRoom()->id;
  QList<int> route = map->search()->findRoute(startRoomId, endRoomId);
  if (route.isEmpty()) {
    showError(QStringLiteral("Could not find route from %1 to %2").arg(startRoomId).arg(endRoomId));
    return;
  }
  ExploreHistory path(map);
  path.goTo(startRoomId);
  for (int step : route) {
    if (step == startRoomId) {
      continue;
    }
    for (const QString& dir : path.currentRoom()->exits.keys()) {
      if (path.currentRoom()->exits.value(dir).dest == step) {
        path.travel(dir);
        break;
      }
    }
  }
  QStringList warnings;
  QStringList messages;
  messages << path.speedwalk(-1, false, &warnings) << "";
  messages += path.describe(-1);
  if (warnings.isEmpty()) {
    showMessage(messages.join("\n"));
  } else {
    showError(messages.join("\n"));
  }
}
