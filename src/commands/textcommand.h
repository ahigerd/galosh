#ifndef GALOSH_TEXTCOMMAND_H
#define GALOSH_TEXTCOMMAND_H

#include <QObject>
#include <QStringList>
#include <QMap>
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

  void invoke(const QStringList& args);

protected:
  virtual int minimumArguments() const { return 0; }
  virtual int maximumArguments() const { return -1; }
  virtual void handleInvoke(const QStringList& args, const KWArgs& kwargs) = 0;

  void addKeyword(const QString& keyword);

  void showMessage(const QString& message);
  void showError(const QString& message);
  void invokeCommand(const QString& command);

  // true = requires parameter, false = no parameter
  QMap<QString, bool> supportedKwargs;

private:
  QStringList m_keywords;
  TextCommandProcessor* m_parent;
};

#endif
