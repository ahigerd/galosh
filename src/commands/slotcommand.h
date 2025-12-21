#ifndef GALOSH_SLOTCOMMAND_H
#define GALOSH_SLOTCOMMAND_H

#include "textcommand.h"
#include <QMetaMethod>
class QObject;

class SlotCommand : public TextCommand
{
public:
  SlotCommand(const QString& name, QObject* obj, const char* slot, const QString& briefHelp, const QString& fullHelp = QString());

  virtual int minimumArguments() const override;
  virtual int maximumArguments() const override;
  virtual QString helpMessage(bool brief) const override;

  using TextCommand::addKeyword;

protected:
  virtual CommandResult handleInvoke(const QStringList& args, const KWArgs& kwargs) override;

private:
  QObject* obj;
  QMetaMethod method0;
  QMetaMethod method1;
  QMetaMethod methodN;
  QString briefHelp, fullHelp;
  int minArgs, maxArgs;
};

#endif
