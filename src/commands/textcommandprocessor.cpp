#include "textcommandprocessor.h"
#include "textcommand.h"
#include "helpcommand.h"

QPair<QString, QStringList> TextCommandProcessor::parseCommand(const QString& command)
{
  QStringList words = command.trimmed().split(' ');
  if (words.isEmpty()) {
    return {};
  }
  QString key = words.takeFirst();
  QStringList args;
  QChar quote = '\0';
  QString token;
  for (const QString& word : words) {
    if (!quote.isNull()) {
      token += " " + word;
      if (word.endsWith(quote)) {
        args << token;
        token.clear();
        quote = '\0';
      }
    } else if (word.isEmpty()) {
      continue;
    } else if (word.startsWith('"') || word.startsWith("'")) {
      quote = word[0];
      token = word;
    } else {
      args << word;
    }
  }
  if (!quote.isNull()) {
    // be permissive: pass through an incomplete quoted string
    args << token;
  }
  return { key, args };
}

TextCommandProcessor::TextCommandProcessor(const QString& commandPrefix)
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

// Returns true if the command was executed
CommandResult TextCommandProcessor::handleCommand(const QString& command)
{
  auto [key, args] = parseCommand(command);
  if (key.isEmpty()) {
    return CommandResult::fail();
  }
  return handleCommand(key.toUpper(), args);
}

CommandResult TextCommandProcessor::handleCommand(const QString& key, const QStringList& args)
{
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
