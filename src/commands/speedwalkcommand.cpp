#include "speedwalkcommand.h"
#include "mapmanager.h"
#include "explorehistory.h"
#include "algorithms.h"

struct Stepper
{
  SpeedwalkCommand* cmd;
  CommandResult result;
  bool validateAll;
  bool quiet;
  QStringList path;
  int dest;

  Stepper(SpeedwalkCommand* cmd, CommandResult result, bool validateAll, bool quiet, const QStringList& path)
  : cmd(cmd), result(result), validateAll(validateAll), quiet(quiet), path(path)
  {
    if (validateAll) {
      const MapRoom* room = cmd->history->currentRoom();
      dest = room->id;
    }
  }

  void nextStep(const CommandResult* stepResult = nullptr) {
    if (result.wasAborted()) {
      delete this;
      return;
    }
    if (stepResult && stepResult->hasError()) {
      result.done(true);
      delete this;
      return;
    }
    if (validateAll && dest > 0) {
      const MapRoom* room = cmd->history->currentRoom();
      if (!room || room->id != dest) {
        if (!room) {
          cmd->showError("Cannot identify current room!");
        } else {
          cmd->showError(QStringLiteral("Currently in room %1 instead of room %2!").arg(room->id).arg(dest));
        }
        result.done(true);
        delete this;
        return;
      }
    }
    if (path.isEmpty()) {
      cmd->showMessage("Speedwalk complete.");
      result.done(false);
      delete this;
      return;
    }
    QString step = path.takeFirst();
    if (validateAll) {
      const MapRoom* room = cmd->history->currentRoom();
      dest = room->exits.value(step.toUpper()).dest;
    }
    CommandResult sub;
    sub.setCallback(this, &Stepper::nextStep);
    cmd->walk(step, sub, false);
  };
};

QStringList SpeedwalkCommand::parseSpeedwalk(const QString& dirs)
{
  if (dirs.isEmpty()) {
    // empty path
    return QStringList();
  }
  if (dirs.back() >= '0' && dirs.back() <= '9') {
    // invalid syntax
    return QStringList();
  }
  QStringList path;
  int count = -1;
  bool bracket = false;
  QString bracketed;
  for (QChar ch : dirs) {
    if (bracket) {
      if (ch == ']') {
        bracket = false;
        while (count-- > 0) {
          path << bracketed;
        }
        bracketed.clear();
      } else {
        bracketed += ch;
      }
    } else if (ch == '[') {
      bracket = true;
      if (count < 0) {
        count = 1;
      }
    } else if (ch >= '0' && ch <= '9') {
      if (count < 0) {
        count = 0;
      }
      count = count * 10 + (ch.cell() - '0');
    } else if (ch == '.' || ch == ' ') {
      // skip dots and spaces
    } else {
      if (count < 0) {
        count = 1;
      }
      while (count-- > 0) {
        path << QString(ch);
      }
    }
  }
  if (bracket) {
    // invalid syntax
    return QStringList();
  }
  return path;
}

SpeedwalkCommand::SpeedwalkCommand(MapManager* map, ExploreHistory* history, WalkFn walkFn, bool quiet)
: TextCommand("SPEEDWALK"), map(map), history(history), walk(walkFn), quiet(quiet)
{
  addKeyword("\x01SPEEDWALK");
  addKeyword("SPEED");
  addKeyword("WALK");
  supportedKwargs["-x"] = true;
  supportedKwargs["-v"] = false;
  supportedKwargs["-f"] = false;
  supportedKwargs["-q"] = false;
  // path has completed successfully -- impossible to type, used recursively
  supportedKwargs["\eok"] = false;
  // path is pre-parsed -- impossible to type, used by other commands
  supportedKwargs[" "] = false;
}

QString SpeedwalkCommand::helpMessage(bool brief) const
{
  if (brief) {
    return "Executes a speedwalk path.";
  }
  return "Executes a speedwalk path.\n"
    "Can also be invoked by prefixing the path with \".\".\n"
    "A speedwalk in progress can be aborted with \"/.\".\n"
    "  -v       Validates each speedwalk step against the automap.\n"
    "  -x [id]  Validates that you are in the specified room ID before moving.\n"
    "           This can be used to halt custom commands that depend on location.\n\n"
    "  -f       Fast mode (incompatible with -v / -x)\n"
    "Speedwalk path syntax:\n"
    "  any letter:  send a one-letter command to the MUD\n"
    "  numbers:     send the next command to the MUD the specified number of times\n"
    "  [command]:   send any command to the MUD\n"
    "Spaces are ignored outside of [].";
}

CommandResult SpeedwalkCommand::handleInvoke(const QStringList& args, const KWArgs& kwargs)
{
  if (kwargs.contains("\eok")) {
    return CommandResult::fail();
  }

  QStringList path;
  bool fast = kwargs.contains("-f");
  bool validateAll = kwargs.contains("-v");
  if (fast && validateAll) {
    showError("Cannot use fast mode with -v.");
    return CommandResult::fail();
  }
  int validate = 0;
  if (kwargs.contains("-x")) {
    validate = kwargs.value("-x").toInt();
    if (!map->room(validate)) {
      showError("Unknown room ID: " + kwargs.value("-x"));
      return CommandResult::fail();
    }
  }

  if (kwargs.contains(" ")) {
    path = args;
  } else {
    QString pathStr = args.join(' ');
    if (pathStr.startsWith('.')) {
      pathStr.remove(0, 1);
    }
    // an empty path is OK if we're validating
    if (!(validate && pathStr.isEmpty())) {
      path = parseSpeedwalk(pathStr);
      if (path.isEmpty()) {
        showError("Invalid speedwalk path");
        return CommandResult::fail();
      }
    }
  }
  const MapRoom* room = history ? history->currentRoom() : nullptr;
  if (!room && (validateAll || validate)) {
    showError("Could not find current room");
    return CommandResult::fail();
  }
  if (validate && room->id != validate) {
    showError(QStringLiteral("Currently in room %1 instead of room %2!").arg(room->id).arg(validate));
    return CommandResult::fail();
  }
  if (fast) {
    CommandResult result;
    for (const QString& step : path) {
      walk(step, result, true);
    }
    result.done(false);
    return result;
  }
  CommandResult result;
  Stepper* stepper = new Stepper(this, result, validateAll, quiet, path);
  stepper->nextStep();
  return result;
}
