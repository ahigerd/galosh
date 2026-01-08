#ifndef GALOSH_ECHOCOMMAND_H
#define GALOSH_ECHOCOMMAND_H

#include "textcommand.h"
class TriggerManager;

class EchoCommand : public TextCommand
{
public:
  EchoCommand(TriggerManager* triggers);

  virtual QString helpMessage(bool brief) const override;

  virtual bool isQuiet() const { return true; }

protected:
  virtual CommandResult handleInvoke(const QStringList& args, const KWArgs& kwargs) override;

private:
  TriggerManager* triggers;
};

#endif
