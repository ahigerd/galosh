#include "routecommand.h"
#include "mapmanager.h"
#include "explorehistory.h"

RouteCommand::RouteCommand(MapManager* map, ExploreHistory* history)
: TextCommand("ROUTE"), map(map), history(history)
{
  supportedKwargs["-q"] = false;
}

QString RouteCommand::helpMessage(bool brief) const
{
  if (brief) {
    return "Calculates the path to a specified room";
  }
  // TODO: better docs
  return "Calculates the path to a specified room.\n"
    "Add '-q' to show only the speedwalking path.";
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
  int endRoomId = args.last().toInt();
  const MapRoom* room = map->room(endRoomId);
  if (!room) {
    showError("Could not find destination room");
    return;
  }
  if (startRoomId == endRoomId) {
    showError("Start room and destination room are the same");
    return;
  }
  map->search()->precompute(true);
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
  messages << path.speedwalk(-1, false, &warnings);
  if (kwargs.contains("-q")) {
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
  } else {
    showError(messages.join("\n"));
  }
}
