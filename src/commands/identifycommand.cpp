#include "identifycommand.h"
#include "itemdatabase.h"

IdentifyCommand::IdentifyCommand(ItemDatabase* db)
: TextCommand("IDENTIFY"), db(db)
{
  addKeyword("ID");
  addKeyword("ITEM");
}

QString IdentifyCommand::helpMessage(bool brief) const
{
  if (brief) {
    return "Shows the recorded stats for an item";
  }
  return
    "Searches the item database for an item by name or index number.\n\n"
    "Words may be regular expressions.\n"
    "Multiple words can be provided. All provided words must be found to match an item.\n"
    "Words connected by a - must be found in order in an item's name.\n"
    "If multiple matching items are found, a list of index numbers and item names will be displayed.";
}

void IdentifyCommand::handleInvoke(const QStringList& args, const KWArgs&)
{
  QList<int> matches;
  if (args.count() == 1) {
    bool ok = false;
    int index = args[0].toInt(&ok);
    if (ok && index > 0) {
      QString name = db->itemName(index);
      if (!name.isEmpty()) {
        matches << index;
      }
    }
  }
  if (matches.isEmpty()) {
    matches = db->searchForName(args);
  }
  if (matches.length() > 1) {
    if (matches.length() > 50) {
      showMessage(QStringLiteral("Found %1 possible matches, showing the first 50:").arg(matches.length()));
    } else {
      showMessage(QStringLiteral("Found %1 possible matches:").arg(matches.length()));
    }
    for (int match : matches.mid(0, 50)) {
      showMessage(QStringLiteral("[%1] %2").arg(match).arg(db->itemName(match)));
    }
  } else if (matches.isEmpty()) {
    showError("No matching items found.");
  } else {
    QString name = db->itemName(matches[0]);
    QString keyword = db->itemKeyword(name);
    showMessage(QStringLiteral("Object name: %1").arg(name));
    if (!keyword.isEmpty()) {
      showMessage(QStringLiteral("Assigned keyword: %1").arg(keyword));
    }
    showMessage(db->itemStats(name).replace("\n", "\r\n"));
  }
  showMessage("");
}
