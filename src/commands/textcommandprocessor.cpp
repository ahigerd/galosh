#include "textcommandprocessor.h"
#include "textcommand.h"
#include "helpcommand.h"

TextCommandProcessor::TextCommandProcessor()
{
  addCommand(new HelpCommand(this));
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
bool TextCommandProcessor::handleCommand(const QString& command)
{
  QStringList words = command.trimmed().split(' ');
  if (words.isEmpty()) {
    return false;
  }
  QString key = words.takeFirst().toUpper();
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
  return handleCommand(key, args);
}

bool TextCommandProcessor::handleCommand(const QString& key, const QStringList& args)
{
  if (commandFilter(key, args)) {
    // the command was handled by the filter
    return true;
  }
  TextCommand* command = m_commands.value(key);
  if (!command) {
    showCommandMessage(nullptr, key + ": unknown command", true);
    return false;
  }
  command->invoke(args);
  return true;
}

bool TextCommandProcessor::commandFilter(const QString&, const QStringList&)
{
  return false;
}
