#ifndef GALOSH_EQUIPMENTCOMMAND_H
#define GALOSH_EQUIPMENTCOMMAND_H

#include "textcommand.h"
#include <QObject>
class GaloshSession;

class EquipmentCommand : public QObject, public TextCommand
{
Q_OBJECT
public:
  EquipmentCommand(GaloshSession* session);

  virtual QString helpMessage(bool brief) const override;

protected:
  virtual int minimumArguments() const override { return 0; }
  virtual int maximumArguments() const override { return 1; }
  virtual void handleInvoke(const QStringList& args, const KWArgs& kwargs) override;

private:
  GaloshSession* session;
};

#endif
