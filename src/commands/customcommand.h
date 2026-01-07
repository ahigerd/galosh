#ifndef GALOSH_CUSTOMCOMMAND_H
#define GALOSH_CUSTOMCOMMAND_H

#include "textcommand.h"
class UserProfile;

class CustomCommand : public TextCommand
{
public:
  CustomCommand(UserProfile* profile);

  virtual QString helpMessage(bool brief) const override;

  virtual bool isHidden() const { return true; }

protected:
  virtual int minimumArguments() const override { return 1; }
  virtual CommandResult handleInvoke(const QStringList& args, const KWArgs& kwargs) override;

private:
  UserProfile* profile;
};

#endif
