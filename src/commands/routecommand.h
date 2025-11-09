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
  virtual void handleInvoke(const QStringList& args, const KWArgs& kwargs) override;

private:
  MapManager* map;
  ExploreHistory* history;
};

#endif
