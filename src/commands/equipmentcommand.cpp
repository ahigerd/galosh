#include "equipmentcommand.h"
#include "galoshsession.h"
#include "itemdatabase.h"

EquipmentCommand::EquipmentCommand(GaloshSession* session)
: QObject(nullptr), TextCommand("EQUIP"), session(session)
{
  addKeyword("EQ");
  supportedKwargs["-u"] = false;
  supportedKwargs["-c"] = true;
}

QString EquipmentCommand::helpMessage(bool brief) const
{
  if (brief) {
    return "Switches equipment sets, or captures a snapshot of your equipment.";
  }
  return "Collects and manages equipment sets.\n"
    "    /EQUIP                Opens the equipment manager.\n"
    "    /EQUIP <set>          Equips the named equipment set.\n"
    "           -c <object>    Puts the removed equipment in the named container.\n"
    "    /EQUIP -u [command]   Captures a snapshot of your equipment by sending\n"
    "                          the specified command. (Default: \"equipment\")";
}

CommandResult EquipmentCommand::handleInvoke(const QStringList& args, const KWArgs& kwargs)
{
  QString arg = args.join(" ");
  if (kwargs.contains("-u")) {
    if (!session->isConnected()) {
      showError("Not connected to server");
      return CommandResult::fail();
    }
    if (arg.isEmpty()) {
      arg = "equipment";
    }
    session->itemDB()->captureEquipment(session->term, [this](const ItemDatabase::EquipmentSet& eq){ session->equipmentReceived(eq); });
    session->term->processCommand(arg);
  } else if (arg.isEmpty()) {
    session->openItemSets();
    return CommandResult::success();
  } else {
    if (!session->isConnected()) {
      showError("Not connected to server");
      return CommandResult::fail();
    }
    QString compare = arg.toLower();
    for (const QString& set : session->profile->itemSets()) {
      if (set.toLower() == compare) {
        return session->switchEquipment(set, kwargs.value("-c"));
      }
    }
    showError(QStringLiteral("No item set \"%1\" found.").arg(arg));
    return CommandResult::fail();
  }
  // unreachable
  return CommandResult::fail();
}
