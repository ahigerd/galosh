#ifndef GALOSH_ZONECOMMAND_H
#define GALOSH_ZONECOMMAND_H

#include "textcommand.h"
class MapManager;

class ZoneCommand : public TextCommand
{
public:
  ZoneCommand(MapManager* map);

  virtual QString helpMessage(bool brief) const override;

protected:
  virtual CommandResult handleInvoke(const QStringList& args, const KWArgs& kwargs) override;

private:
  MapManager* map;
};

#endif
