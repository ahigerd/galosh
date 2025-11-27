#ifndef GALOSH_TEXTCOMMANDPROCESSOR_H
#define GALOSH_TEXTCOMMANDPROCESSOR_H

#include <QStringList>
#include <QMap>
class TextCommand;

class TextCommandProcessor
{
public:
  TextCommandProcessor(const QString& commandPrefix = QString());
  virtual ~TextCommandProcessor();

  void addCommand(TextCommand* command);

  template <typename T>
  T* addCommand(T* command) {
    addCommand(static_cast<TextCommand*>(command));
    return command;
  }

  void help();
  bool handleCommand(const QString& command);
  bool handleCommand(const QString& command, const QStringList& args);

protected:
  // if command is nullptr, command was unrecognized
  virtual void showCommandMessage(TextCommand* command, const QString& message, bool isError) = 0;
  // like eventFilter, return true to stop further processing
  virtual bool commandFilter(const QString& command, const QStringList& args);

private:
  friend class TextCommand;
  friend class HelpCommand;
  QMap<QString, TextCommand*> m_commands;
  TextCommand* m_helpCommand;
};

#endif
