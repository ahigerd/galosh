#ifndef GALOSH_SENDCOMMAND_H
#define GALOSH_SENDCOMMAND_H

#include "textcommand.h"
class GaloshWindow;

class SendCommand : public TextCommand
{
public:
  SendCommand(GaloshWindow* win);

  virtual QString helpMessage(bool brief) const override;

  virtual bool isQuiet() const { return true; }
  virtual bool isHidden() const { return true; }

protected:
  virtual int minimumArguments() const override { return 2; }
  virtual int maximumArguments() const override { return 2; }
  virtual CommandResult handleInvoke(const QStringList& args, const KWArgs& kwargs) override;

private:
  GaloshWindow* win;
};

#endif
