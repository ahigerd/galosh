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

  static QString speedwalkPrefix;
  static QString sendPrefix;

  TextCommandProcessor(const QString& commandPrefix = QString());
  virtual ~TextCommandProcessor();

  void addCommand(TextCommand* command);

  template <typename T>
  T* addCommand(T* command) {
    addCommand(static_cast<TextCommand*>(command));
    return command;
  }

  void help();

  QPair<QString, QStringList> parseCommand(const QString& command);

  inline bool insertCommands(const QStringList& commands, bool forgive = false) { return enqueueCommands(commands, true, forgive); }
  inline bool enqueueCommands(const QStringList& commands, bool forgive = false) { return enqueueCommands(commands, false, forgive); }
  inline bool enqueueCommand(const QString& command, bool forgive = false) { return enqueueCommands({ command }, false, forgive); }
  bool abortQueue(bool quiet = false);
  bool isRunning() const;

protected:
  // if command is nullptr, command was unrecognized
  virtual void showCommandMessage(TextCommand* command, const QString& message, MessageType messageType) = 0;
  // like eventFilter, return true to stop further processing
  virtual bool commandFilter(const QString& command, const QStringList& args);
  // call from within commandFilter() or TextCommand::invokeCommand() to signal an error
  void commandErrored();
  // return true if the command matches a custom command
  virtual bool isCustomCommand(const QString& command) const;

private:
  friend class TextCommand;
  friend class HelpCommand;

  bool enqueueCommands(const QStringList& commands, bool before, bool forgive);
  CommandResult handleCommand(const QString& command, const QStringList& args);
  void handleNext(const CommandResult* result = nullptr);

  QMap<QString, TextCommand*> m_commands;
  QMap<int, TextCommand*> m_callbacks;
  QString m_commandPrefix;
  QList<QPair<QString, QStringList>> m_commandQueue;
  CommandResult m_pending;
  TextCommand* m_pendingCommand;
  TextCommand* m_helpCommand;
  bool m_isRunningSync;
  bool m_hasError;
};

#endif
