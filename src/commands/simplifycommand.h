#ifndef GALOSH_SIMPLIFYCOMMAND_H
#define GALOSH_SIMPLIFYCOMMAND_H

#include "textcommand.h"
class ExploreHistory;

class SimplifyCommand : public TextCommand
{
public:
  SimplifyCommand(ExploreHistory* history);

  virtual QString helpMessage(bool brief) const override;
  virtual int maximumArguments() const override { return 0; }

protected:
  virtual void handleInvoke(const QStringList& args, const KWArgs& kwargs) override;

private:
  ExploreHistory* history;
};

#endif
