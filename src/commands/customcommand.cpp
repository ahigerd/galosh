#include "customcommand.h"
#include "userprofile.h"
#include "textcommandprocessor.h"
#include <QtDebug>

CustomCommand::CustomCommand(UserProfile* profile)
: TextCommand("CUSTOM"), profile(profile)
{
  // initializers only
}

QString CustomCommand::helpMessage(bool) const
{
  // First parameter: command name
  // Second parameter: raw input
  // Rest of parameters: parsed arguments
  return "Executes a custom command.";
}

CommandResult CustomCommand::handleInvoke(const QStringList& _args, const KWArgs&)
{
  QStringList args = _args;
  QString cmd = args.takeFirst();
  QString raw = args.takeFirst();

  QStringList actions = profile->customCommand(cmd);
  if (actions.isEmpty()) {
    showError("Unknown custom command: " + cmd);
    return CommandResult::fail();
  }

  static QRegularExpression argRE("(?<!%)%(\\d+|[*]|{\\d+[:][^}]+})([+]?)");
  QStringList parsed;
  for (QString action : actions) {
    int pos = 0;
    while (pos < action.length()) {
      auto match = argRE.match(action, pos);
      if (!match.hasMatch()) {
        break;
      }
      QString ref = match.captured(1);
      QString replacement = ref.section(':', 1);
      replacement.chop(1);
      int index = ref.section(':', 0, 0).toInt() - 1;
      if (index < 0) {
        index = 0;
      }
      bool rest = !match.captured(2).isEmpty();
      if (index < args.length()) {
        if (rest) {
          replacement = args.mid(index).join(' ');
        } else {
          replacement = args[index];
        }
      }
      action = action.left(match.capturedStart(0)) + replacement + action.mid(match.capturedEnd(0));
      pos = match.capturedStart(0) + replacement.length();
    }
    parsed << action;
  }
  if (!parsed.isEmpty()) {
    parsed.insert(0, "\x01PUSH " + cmd);
    parsed << ("\x01POP");
  }
  processor()->insertCommands(parsed);
  return CommandResult::success();
}
