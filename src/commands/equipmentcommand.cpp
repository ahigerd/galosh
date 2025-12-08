#include "equipmentcommand.h"
#include "galoshsession.h"
#include "itemdatabase.h"

EquipmentCommand::EquipmentCommand(GaloshSession* session)
: QObject(nullptr), TextCommand("EQUIPMENT"), session(session)
{
  addKeyword("EQ");
  addKeyword("EQUIP");
}

QString EquipmentCommand::helpMessage(bool brief) const
{
  if (brief) {
    return "Captures a snapshot of your current equipment.";
  }
  return "Captures a snapshot of your current equipment.\n"
    "This sends the \"eq\" command to the MUD.\n"
    "Any provided arguments will be added to the sent command.";
}

void EquipmentCommand::handleInvoke(const QStringList& args, const KWArgs&)
{
  if (!session->isConnected()) {
    showError("Not connected to server");
    return;
  }
  session->itemDB()->captureEquipment();
  QStringList command = args;
  command.insert(0, "eq");
  session->term->processCommand(command.join(" "));
}
