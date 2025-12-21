#ifndef GALOSH_MAPSEARCHCOMMAND_H
#define GALOSH_MAPSEARCHCOMMAND_H

#include "textcommand.h"
class MapManager;

class MapSearchCommand : public TextCommand
{
public:
  MapSearchCommand(MapManager* map);

  virtual QString helpMessage(bool brief) const override;

protected:
  virtual int minimumArguments() const override { return 1; }
  virtual CommandResult handleInvoke(const QStringList& args, const KWArgs& kwargs) override;

private:
  MapManager* map;
};

#endif
