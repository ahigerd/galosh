#include "equipmentcommand.h"
#include "galoshsession.h"
#include "itemdatabase.h"

EquipmentCommand::EquipmentCommand(GaloshSession* session)
: QObject(nullptr), TextCommand("EQUIP"), session(session)
{
  addKeyword("EQ");
  supportedKwargs["-u"] = false;
}

QString EquipmentCommand::helpMessage(bool brief) const
{
  if (brief) {
    return "Switches equipment sets, or captures a snapshot of your equipment.";
  }
  return "Collects and manages equipment sets.\n"
    "    /EQUIP                Opens the equipment manager.\n"
    "    /EQUIP <set>          Equips the named equipment set.\n"
    "    /EQUIP -u [command]   Captures a snapshot of your equipment\n"
    "                          by sending the specified command.\n"
    "                          (Default: \"equipment\")";
}

void EquipmentCommand::handleInvoke(const QStringList& args, const KWArgs& kwargs)
{
  QString arg = args.join(" ");
  if (kwargs.contains("-u")) {
    if (!session->isConnected()) {
      showError("Not connected to server");
      return;
    }
    if (arg.isEmpty()) {
      arg = "equipment";
    }
    session->itemDB()->captureEquipment(session->term, [this](const QList<ItemDatabase::EquipSlot>& eq){ session->equipmentReceived(eq); });
    session->term->processCommand(arg);
  } else if (arg.isEmpty()) {
    session->openEquipment();
  } else {
    if (!session->isConnected()) {
      showError("Not connected to server");
      return;
    }
    session->switchEquipment(arg);
  }
}
