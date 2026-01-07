#ifndef GALOSH_ROUTECOMMAND_H
#define GALOSH_ROUTECOMMAND_H

#include "textcommand.h"
class MapManager;
class ExploreHistory;

class RouteCommand : public TextCommand
{
public:
  RouteCommand(MapManager* map, ExploreHistory* history);

  virtual QString helpMessage(bool brief) const override;

protected:
  virtual int minimumArguments() const override { return 1; }
  virtual int maximumArguments() const override { return 2; }
  virtual CommandResult handleInvoke(const QStringList& args, const KWArgs& kwargs) override;

private:
  MapManager* map;
  ExploreHistory* history;
};

#endif
