#include "simplifycommand.h"
#include "explorehistory.h"

SimplifyCommand::SimplifyCommand(ExploreHistory* history)
: TextCommand("SIMPLIFY"), history(history)
{
  supportedKwargs["-a"] = false;
}

QString SimplifyCommand::helpMessage(bool brief) const
{
  if (brief) {
    return "Removes backtracking from exploration history";
  }
  return
    "Simplifies the exploration history by removing backtracking.\n"
    "Add '-a' to more aggressively look for shortcuts between rooms.";
}

void SimplifyCommand::handleInvoke(const QStringList&, const KWArgs& kwargs)
{
  int before = history->length();
  history->simplify(kwargs.contains("-a"));
  int after = history->length();
  if (before == after) {
    if (kwargs.contains("-a")) {
      showError("No backtracking or shortcuts in exploration history");
    } else {
      showError("No backtracking in exploration history");
    }
  } else {
    showMessage(QStringLiteral("Simplified history from %1 steps to %2 steps").arg(before).arg(after));
  }
}
