#ifndef GALOSH_HELPCOMMAND_H
#define GALOSH_HELPCOMMAND_H

#include "textcommand.h"

class HelpCommand : public TextCommand
{
public:
  HelpCommand(TextCommandProcessor* source);

  virtual int maximumArguments() const override { return 1; }
  virtual QString helpMessage(bool brief) const override;

protected:
  virtual void handleInvoke(const QStringList& args, const KWArgs& kwargs) override;

private:
  TextCommandProcessor* source;
};

#endif
