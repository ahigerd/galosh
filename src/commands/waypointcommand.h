#ifndef GALOSH_WAYPOINTCOMMAND_H
#define GALOSH_WAYPOINTCOMMAND_H

#include <QObject>
#include "textcommand.h"
class MapManager;
class ExploreHistory;

class WaypointCommand : public QObject, public TextCommand
{
Q_OBJECT
public:
  WaypointCommand(MapManager* map, ExploreHistory* history);

  virtual QString helpMessage(bool brief) const override;

signals:
  void speedwalk(const QStringList& steps);

protected:
  virtual int minimumArguments() const override { return 0; }
  virtual int maximumArguments() const override { return 1; }
  virtual void handleInvoke(const QStringList& args, const KWArgs& kwargs) override;

private:
  void handleAdd(const QString& name, int roomId);
  void handleDelete(const QString& name);
  void handleRoute(const QString& name, bool run);

  MapManager* map;
  ExploreHistory* history;
};

#endif
