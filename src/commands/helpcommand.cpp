#include "helpcommand.h"
#include "textcommandprocessor.h"

HelpCommand::HelpCommand(TextCommandProcessor* source)
: TextCommand("HELP"), source(source)
{
  addKeyword("H");
}

QString HelpCommand::helpMessage(bool brief) const
{
  if (brief) {
    return "Shows information about a command";
  }
  return
    "Shows the list of commands.\n"
    "If a command name is provided, shows more detailed help for that command.";
}

void HelpCommand::handleInvoke(const QStringList& args, const KWArgs&)
{
  if (args.isEmpty()) {
    QMap<QString, const TextCommand*> names;
    for (const TextCommand* cmd : source->m_commands) {
      names[cmd->name()] = cmd;
    }
    for (QString name : names.keys()) {
      const TextCommand* cmd = names[name];
      showMessage(QStringLiteral("%1\t%2").arg(name.leftJustified(8)).arg(cmd->helpMessage(true)));
    }
    return;
  }
  QString name = args.first().toUpper();
  const TextCommand* cmd = source->m_commands.value(name);
  if (!cmd) {
    showError("Unknown command: " + name);
    return;
  }
  QString intro = cmd->name();
  if (cmd->keywords().length() > 1) {
    if (cmd->keywords().length() > 2) {
      intro += " (aliases: ";
    } else {
      intro += " (alias: ";
    }
    bool first = true;
    for (const QString& alias : cmd->keywords()) {
      if (alias != cmd->name()) {
        if (first) {
          first = false;
        } else {
          intro += ", ";
        }
        intro += alias;
      }
    }
  }

  showMessage(cmd->helpMessage(false));
}
