#ifndef GALOSH_TEXTCOMMANDPROCESSOR_H
#define GALOSH_TEXTCOMMANDPROCESSOR_H

#include <QStringList>
#include <QMap>
#include "commandresult.h"
class TextCommand;

class TextCommandProcessor
{
public:
  enum MessageType {
    MT_Message,
    MT_Error,
    MT_Subcommand,
  };

  static QPair<QString, QStringList> parseCommand(const QString& command);

  TextCommandProcessor(const QString& commandPrefix = QString());
  virtual ~TextCommandProcessor();

  void addCommand(TextCommand* command);

  template <typename T>
  T* addCommand(T* command) {
    addCommand(static_cast<TextCommand*>(command));
    return command;
  }

  void help();
  CommandResult handleCommand(const QString& command);
  CommandResult handleCommand(const QString& command, const QStringList& args);

protected:
  // if command is nullptr, command was unrecognized
  virtual void showCommandMessage(TextCommand* command, const QString& message, MessageType messageType) = 0;
  // like eventFilter, return true to stop further processing
  virtual bool commandFilter(const QString& command, const QStringList& args);
  // call from within commandFilter() or TextCommand::invokeCommand() to signal an error
  void commandErrored();

private:
  friend class TextCommand;
  friend class HelpCommand;
  QMap<QString, TextCommand*> m_commands;
  QMap<int, TextCommand*> m_callbacks;
  TextCommand* m_helpCommand;
  bool m_hasError;
};

#endif
