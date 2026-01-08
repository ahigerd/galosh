#include "echocommand.h"
#include "triggermanager.h"
#include <QRegularExpression>

static QRegularExpression validColor("^(?:[0-9]+;)*[0-9]$");

EchoCommand::EchoCommand(TriggerManager* triggers)
: TextCommand("ECHO"), triggers(triggers)
{
  supportedKwargs["-n"] = false;
  supportedKwargs["-c"] = true;
}

QString EchoCommand::helpMessage(bool brief) const
{
  if (brief) {
    return "Displays a message and processes triggers for it.";
  }
  return "/ECHO [-n] [-c <color>] <message>\n"
    "Displays the specified message and processes triggers for it.\n"
    "    -n    Don't process triggers\n"
    "    -c    An ANSI color code (without ESC[ or m, default 1;96)";
}

CommandResult EchoCommand::handleInvoke(const QStringList& args, const KWArgs& kwargs)
{
  QString line = args.join(" ");
  QString color = "\x1b[0;1m";
  if (kwargs.contains("-c")) {
    if (!validColor.match(kwargs["-c"]).hasMatch()) {
      showError("Invalid color code: " + kwargs["-c"]);
      return CommandResult::fail();
    }
    color = "\x1b[0;" + kwargs["-c"] + "m";
  }
  showMessage(color + line);
  if (triggers && !kwargs.contains("-n")) {
    triggers->processLine(line);
  }
  return CommandResult::success();
}
