#ifndef GALOSH_SPEEDWALKCOMMAND_H
#define GALOSH_SPEEDWALKCOMMAND_H

#include "textcommand.h"
#include <QObject>
#include <functional>
class MapManager;
class ExploreHistory;

class SpeedwalkCommand : public QObject, public TextCommand
{
Q_OBJECT
public:
  using WalkFn = std::function<void(const QString&, CommandResult&, bool)>;
  static QStringList parseSpeedwalk(const QString& dirs);

  SpeedwalkCommand(MapManager* map, ExploreHistory* history, WalkFn walkFn, bool quiet = true);

  virtual QString helpMessage(bool brief) const override;

  virtual bool isAsync() const { return true; }

signals:
  void speedwalk(const QStringList& steps, bool fast);

protected:
  virtual int minimumArguments() const override { return 1; }
  virtual CommandResult handleInvoke(const QStringList& args, const KWArgs& kwargs) override;

private:
  friend class Stepper;
  MapManager* map;
  ExploreHistory* history;
  WalkFn walk;
  bool quiet;
};

#endif
