#include "equipmentcommand.h"
#include "galoshsession.h"
#include "itemdatabase.h"

EquipmentCommand::EquipmentCommand(GaloshSession* session)
: QObject(nullptr), TextCommand("EQUIPMENT"), session(session)
{
  addKeyword("EQ");
  addKeyword("EQUIP");
  supportedKwargs["-c"] = false;
}

QString EquipmentCommand::helpMessage(bool brief) const
{
  if (brief) {
    return "Captures a snapshot of your current equipment.";
  }
  return "Captures a snapshot of your current equipment.\n"
    "This sends the \"eq\" command to the MUD.\n"
    "Any provided arguments will be added to the sent command."
    "    -c <command>   Send <command> instead of \"eq\"";
}

void EquipmentCommand::handleInvoke(const QStringList& args, const KWArgs& kwargs)
{
  if (!session->isConnected()) {
    showError("Not connected to server");
    return;
  }
  session->itemDB()->captureEquipment(session->term, [this](const QList<ItemDatabase::EquipSlot>& eq){ session->equipmentReceived(eq); });
  QStringList command = args;
  if (!kwargs.contains("-c")) {
    command.insert(0, "eq");
  }
  session->term->processCommand(command.join(" "));
}
