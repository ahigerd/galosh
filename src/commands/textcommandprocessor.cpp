#include "textcommandprocessor.h"
#include "textcommand.h"
#include "helpcommand.h"
#include <QtDebug>

QString TextCommandProcessor::speedwalkPrefix = ".";
QString TextCommandProcessor::sendPrefix = ":";

QPair<QString, QStringList> TextCommandProcessor::parseCommand(const QString& _command)
{
  QString command = _command.trimmed();

  if (command != speedwalkPrefix && command.startsWith(speedwalkPrefix)) {
    return { "SPEEDWALK", { command.mid(speedwalkPrefix.length()) } };
  } else if (command.startsWith("\\" + speedwalkPrefix)) {
    return { "", { command.mid(1) } };
  }

  if (m_commands.contains("SEND")) {
    if (command != sendPrefix && command.startsWith(sendPrefix)) {
      int space = command.indexOf(' ');
      if (space > sendPrefix.length()) {
        QString target = command.mid(sendPrefix.length(), space - sendPrefix.length());
        command = command.mid(space + 1);
        return { "SEND", { target, command } };
      } else {
        return { "", { command } };
      }
    } else if (command.startsWith("\\" + sendPrefix)) {
      return { "", { command.mid(1) } };
    }
  }

  QString key = command.section(' ', 0, 0);
  QStringList args;
  if (m_commands.contains("CUSTOM") && isCustomCommand(key)) {
    args << key << command;
    key = "CUSTOM";
  } else if (!key.startsWith(m_commandPrefix) && !isInternalCommand(key)) {
    return { "", { command } };
  } else if (!isInternalCommand(key)) {
    key = key.mid(m_commandPrefix.length()).toUpper();
  }
  command = command.section(' ', 1);

  if (key.startsWith("\x01")) {
    return { key, { command } };
  }

  QChar quote = '\0';
  QString token;
  bool escape = false;

  for (const QChar& ch : command) {
    if (escape) {
      escape = false;
      token += ch;
    } else if (ch == '\\') {
      escape = true;
    } else if (quote == ch || (quote.isNull() && ch.isSpace())) {
      if (quote == ch || !token.isEmpty()) {
        args << token;
      }
      token.clear();
      quote = '\0';
    } else if (quote.isNull() && (ch == '"' || ch == '\'')) {
      quote = ch;
    } else {
      token += ch;
    }
  }
  if (!token.isEmpty()) {
    args << token;
  }
  return { key, args };
}

TextCommandProcessor::TextCommandProcessor(const QString& commandPrefix)
: m_commandPrefix(commandPrefix), m_pending(CommandResult::success()), m_pendingCommand(nullptr), m_isRunningSync(false)
{
  m_helpCommand = new HelpCommand(commandPrefix, this);
  addCommand(m_helpCommand);
}

TextCommandProcessor::~TextCommandProcessor()
{
  for (const QString& key : m_commands.keys()) {
    TextCommand* command = m_commands.value(key);
    if (command) {
      // ~TextCommand will remove aliases from the map
      delete command;
    }
  }
}

void TextCommandProcessor::addCommand(TextCommand* command)
{
  command->setParent(this);
  for (const QString& key : command->keywords()) {
    m_commands[key] = command;
  }
}

CommandResult TextCommandProcessor::handleCommand(const QString& key, const QStringList& args)
{
  if (key == "\x01PUSH") {
    if (!args.isEmpty()) {
      m_customStack << args.first();
    }
    return CommandResult::success();
  } else if (key == "\x01POP") {
    if (!m_customStack.isEmpty()) {
      m_customStack.takeLast();
    }
    return CommandResult::success();
  } else if (key == "CUSTOM" && args.length() > 1) {
    if (m_customStack.contains(args.first())) {
      return handleCommand("", { args[1] });
    }
  }
  m_hasError = false;
  if (commandFilter(key, args)) {
    // the command was handled by the filter
    return CommandResult::success();
  }
  TextCommand* command = m_commands.value(key);
  if (!command) {
    showCommandMessage(nullptr, key + ": unknown command", MT_Error);
    return CommandResult::fail();
  }
  m_pendingCommand = command;
  return command->invoke(args);
}

void TextCommandProcessor::help()
{
  m_helpCommand->invoke({});
}

bool TextCommandProcessor::commandFilter(const QString&, const QStringList&)
{
  return false;
}

void TextCommandProcessor::commandErrored()
{
  m_hasError = true;
}

bool TextCommandProcessor::isCustomCommand(const QString&) const
{
  return false;
}

bool TextCommandProcessor::isInternalCommand(const QString& command) const
{
  if (command.startsWith('\x01')) {
    return true;
  }
  if (m_commands.contains(command)) {
    return m_commands.value(command)->isHidden();
  }
  return false;
}

bool TextCommandProcessor::isRunning() const
{
  return m_isRunningSync || !m_commandQueue.isEmpty() || !m_pending.isFinished();
}

bool TextCommandProcessor::enqueueCommands(const QStringList& commands, bool before, bool forgive)
{
  bool ready = !isRunning();
  int start = 0, end = commands.length(), step = 1;
  if (before) {
    start = end - 1;
    end = -1;
    step = -1;
  }
  for (int i = start; i != end; i += step) {
    auto [cmd, args] = parseCommand(commands[i]);
    if (!cmd.isEmpty() && !isCustomCommand(cmd) && !isInternalCommand(cmd)) {
      cmd = cmd.toUpper();
      if (!m_commands.contains(cmd)) {
        showCommandMessage(nullptr, m_commandPrefix + cmd + ": unknown command", MT_Error);
        if (!forgive) {
          abortQueue(true);
        }
        return false;
      }
    }
    if (before) {
      m_commandQueue.insert(0, { cmd, args });
    } else {
      m_commandQueue << qMakePair(cmd, args);
    }
  }
  if (ready) {
    handleNext();
  }
  return true;
}

bool TextCommandProcessor::abortQueue(bool quiet)
{
  if (!quiet && !isRunning()) {
    return false;
  }
  m_customStack.clear();
  m_commandQueue.clear();
  if (!m_pending.isFinished()) {
    m_pending.abort();
  }
  if (!quiet) {
    showCommandMessage(m_pendingCommand, "Aborted.", MT_Error);
  }
  m_pendingCommand = nullptr;
  return true;
}

void TextCommandProcessor::handleNext(const CommandResult* result)
{
  if (result && (result->wasAborted() || result->hasError())) {
    if (!result->wasAborted()) {
      if (!m_commandQueue.isEmpty()) {
        showCommandMessage(nullptr, "Command aborted due to error.", MT_Error);
      }
    }
    abortQueue(true);
    return;
  }
  m_isRunningSync = true;
  while (!m_commandQueue.isEmpty()) {
    m_pendingCommand = nullptr;
    auto [cmd, args] = m_commandQueue.takeFirst();
    m_pending = handleCommand(cmd, args);
    if (!m_pending.isFinished()) {
      m_pending.setCallback(this, &TextCommandProcessor::handleNext);
      break;
    }
  }
  m_isRunningSync = false;
}

bool TextCommandProcessor::isCommandQuiet(const QString& command) const
{
  if (isInternalCommand(command)) {
    return true;
  }
  TextCommand* cmd = m_commands.value(command);
  if (!cmd) {
    return false;
  }
  return cmd->isQuiet();
}
