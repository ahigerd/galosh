#include "textcommand.h"
#include "textcommandprocessor.h"

TextCommand::TextCommand(const QString& name)
: m_parent(nullptr)
{
  addKeyword(name);
}

TextCommand::~TextCommand()
{
  if (m_parent) {
    for (const QString& keyword : m_keywords) {
      m_parent->m_commands.remove(keyword);
    }
  }
}

void TextCommand::setParent(TextCommandProcessor* parent)
{
  m_parent = parent;
}

void TextCommand::addKeyword(const QString& name)
{
  m_keywords << name.toUpper();
  if (m_parent) {
    m_parent->m_commands[name.toUpper()] = this;
  }
}

CommandResult TextCommand::invoke(const QStringList& args)
{
  KWArgs kwargs;
  QStringList positional;
  int argCount = args.count();
  if (supportedKwargs.isEmpty()) {
    positional = args;
  } else {
    for (int i = 0; i < argCount; i++) {
      QString arg = args[i].toLower();
      if (supportedKwargs.contains(arg)) {
        if (supportedKwargs.value(arg)) {
          if (i == argCount - 1) {
            showError(QStringLiteral("%1 requires an argument").arg(arg));
            return CommandResult::fail();
          }
          kwargs[arg] = args[++i];
        } else {
          kwargs[arg] = arg;
        }
      } else {
        positional << args[i];
      }
    }
  }
  argCount = positional.count();
  if (argCount < minimumArguments()) {
    showError("missing required argument");
    return CommandResult::fail();
  }
  int maxArgs = maximumArguments();
  if (maxArgs >= 0 && argCount > maxArgs) {
    showError("unexpected arguments");
    return CommandResult::fail();
  }
  return handleInvoke(positional, kwargs);
}

void TextCommand::showMessage(const QString& message)
{
  Q_ASSERT(m_parent);
  m_parent->showCommandMessage(this, message, TextCommandProcessor::MT_Message);
}

void TextCommand::showError(const QString& message)
{
  Q_ASSERT(m_parent);
  m_parent->commandErrored();
  m_parent->showCommandMessage(this, message, TextCommandProcessor::MT_Error);
}

void TextCommand::showCommand(const QString& message)
{
  Q_ASSERT(m_parent);
  m_parent->showCommandMessage(this, message, TextCommandProcessor::MT_Subcommand);
}

CommandResult TextCommand::invokeCommand(const QString& command, const QStringList& args, bool quiet)
{
  Q_ASSERT(m_parent);
  if (!quiet) {
    QString message = command;
    if (!args.isEmpty()) {
      message += " " + args.join(" ");
    }
    showCommand(message);
  }
  return m_parent->handleCommand(command, args);
}
