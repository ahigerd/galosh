#include "helpcommand.h"
#include "textcommandprocessor.h"

HelpCommand::HelpCommand(const QString& commandPrefix, TextCommandProcessor* source)
: TextCommand("HELP"), source(source), commandPrefix(commandPrefix)
{
  addKeyword("H");
  addKeyword("?");
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
      showMessage(QStringLiteral("%1%2\t%3").arg(commandPrefix).arg(name.leftJustified(8)).arg(cmd->helpMessage(true)));
    }
    return;
  }
  QString name = args.first().toUpper();
  if (!commandPrefix.isEmpty() && name.startsWith(commandPrefix)) {
    name = name.mid(commandPrefix.length());
  }
  const TextCommand* cmd = source->m_commands.value(name);
  if (!cmd) {
    showError("Unknown command: " + name);
    return;
  }
  QString intro = QStringLiteral("Help for %1%2").arg(commandPrefix).arg(cmd->name());
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
        intro += commandPrefix + alias;
      }
    }
    if (!first) {
      intro += ")";
    }
  }

  showMessage(intro + "\n" + QString(intro.length(), '='));
  showMessage(cmd->helpMessage(false) + "\n");
}
