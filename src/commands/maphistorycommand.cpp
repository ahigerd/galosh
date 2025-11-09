#include "maphistorycommand.h"
#include "explorehistory.h"

MapHistoryCommand::MapHistoryCommand(const QString& cmd, ExploreHistory* history)
: TextCommand(cmd), history(history)
{
  defaultReverse = cmd == "REVERSE";
  defaultSpeed = cmd == "SPEED";
  if (defaultReverse) {
    addKeyword("REV");
  }
  supportedKwargs["-r"] = false;
  supportedKwargs["-s"] = false;
}

QString MapHistoryCommand::helpMessage(bool brief) const
{
  if (brief) {
    if (defaultReverse) {
      return "Shows backtracking steps for the exploration history";
    } else if (defaultSpeed) {
      return "Shows speedwalking directions for the exploration history";
    } else {
      return "Shows the exploration history";
    }
  }
  QString message = helpMessage(true) + "\n\n";
  if (!defaultSpeed) {
    message += "By default, shows the last 10 steps. Specify a number to show more or less, or\n"
      "use '*' or 'ALL' to show the complete exploration history.";
  }
  if (!defaultReverse) {
    message += "\nAdd '-r' to show backtracking steps.";
  }
  if (!defaultSpeed) {
    message += "\nAdd '-s' to show the history as speedwalking directions.Speedwalking directions\n"
      "will include the complete exploration history if a length is not specified.";
  }
  return message;
}

void MapHistoryCommand::handleInvoke(const QStringList& args, const KWArgs& kwargs)
{
  bool speedwalk = defaultSpeed || kwargs.contains("-s");
  bool reverse = defaultReverse || kwargs.contains("-r");
  int len;
  if (args.isEmpty()) {
    len = speedwalk ? -1 : 0;
  } else if (args[0] == "*" || args[0].toUpper() == "ALL") {
    len = -1;
  } else {
    len = args[0].toInt();
    if (len <= 0) {
      showError("bad history length: " + args[0]);
      return;
    }
  }

  QStringList messages;
  bool error = false;
  if (speedwalk) {
    QString dirs = history->speedwalk(len, reverse, &messages);
    if (dirs.length() > 1) {
      messages << dirs;
    } else {
      error = true;
    }
  } else {
    messages = history->describe(len, reverse);
  }
  if (messages.isEmpty()) {
    showError("No history to show");
    return;
  }
  if (error) {
    showError(messages.join("\n"));
  } else {
    showMessage(messages.join("\n"));
  }
}
