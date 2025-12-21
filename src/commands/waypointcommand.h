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
  virtual CommandResult handleInvoke(const QStringList& args, const KWArgs& kwargs) override;

private:
  CommandResult handleAdd(const QString& name, int roomId);
  CommandResult handleDelete(const QString& name);
  CommandResult handleRoute(const QString& name, bool run, bool fast);

  MapManager* map;
  ExploreHistory* history;
};

#endif
