#ifndef GALOSH_MAPHISTORYCOMMAND_H
#define GALOSH_MAPHISTORYCOMMAND_H

#include "textcommand.h"
class ExploreHistory;

class MapHistoryCommand : public TextCommand
{
public:
  MapHistoryCommand(const QString& cmd, ExploreHistory* history);

  virtual QString helpMessage(bool brief) const override;
  virtual int maximumArguments() const override { return 1; }

protected:
  virtual CommandResult handleInvoke(const QStringList& args, const KWArgs& kwargs) override;

private:
  ExploreHistory* history;
  bool defaultReverse;
  bool defaultSpeed;
};

#endif
