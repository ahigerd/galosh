#ifndef GALOSH_TEXTCOMMAND_H
#define GALOSH_TEXTCOMMAND_H

#include <QObject>
#include <QStringList>
#include <QMap>
#include "commandresult.h"
class TextCommandProcessor;

class TextCommand
{
public:
  using KWArgs = QMap<QString, QString>;

  TextCommand(const QString& name);
  virtual ~TextCommand();

  void setParent(TextCommandProcessor* parent);

  inline QString name() const { return m_keywords.first(); }
  inline QStringList keywords() const { return m_keywords; }
  virtual QString helpMessage(bool brief) const = 0;
  virtual bool isHidden() const { return false; }

  CommandResult invoke(const QStringList& args);

protected:
  virtual int minimumArguments() const { return 0; }
  virtual int maximumArguments() const { return -1; }
  virtual CommandResult handleInvoke(const QStringList& args, const KWArgs& kwargs) = 0;

  void addKeyword(const QString& keyword);

  void showMessage(const QString& message);
  void showError(const QString& message);
  void showCommand(const QString& message);
  CommandResult invokeCommand(const QString& command, const QStringList& args, bool quiet = true);

  void finished(bool error);

  TextCommandProcessor* processor() const { return m_parent; }

  // true = requires parameter, false = no parameter
  QMap<QString, bool> supportedKwargs;

private:
  QStringList m_keywords;
  TextCommandProcessor* m_parent;
};

#endif
