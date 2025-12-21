#ifndef GALOSH_IDENTIFYCOMMAND_H
#define GALOSH_IDENTIFYCOMMAND_H

#include "textcommand.h"
class ItemDatabase;

class IdentifyCommand : public TextCommand
{
public:
  IdentifyCommand(ItemDatabase* db);

  virtual int minimumArguments() const override { return 1; }
  virtual QString helpMessage(bool brief) const override;

protected:
  virtual CommandResult handleInvoke(const QStringList& args, const KWArgs& kwargs) override;

private:
  ItemDatabase* db;
};

#endif
