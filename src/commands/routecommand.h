#ifndef GALOSH_ROUTECOMMAND_H
#define GALOSH_ROUTECOMMAND_H

#include "textcommand.h"
#include <QObject>
class MapManager;
class ExploreHistory;

class RouteCommand : public QObject, public TextCommand
{
Q_OBJECT
public:
  RouteCommand(MapManager* map, ExploreHistory* history);

  virtual QString helpMessage(bool brief) const override;

signals:
  void speedwalk(const QStringList& steps);

protected:
  virtual int minimumArguments() const override { return 1; }
  virtual int maximumArguments() const override { return 2; }
  virtual void handleInvoke(const QStringList& args, const KWArgs& kwargs) override;

private:
  MapManager* map;
  ExploreHistory* history;
};

#endif
