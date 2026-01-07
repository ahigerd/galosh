#include "sendcommand.h"
#include "galoshwindow.h"

SendCommand::SendCommand(GaloshWindow* win)
: TextCommand("SEND"), win(win)
{
  // initializers only
}

QString SendCommand::helpMessage(bool) const
{
  return "Sends a command to another session.";
}

CommandResult SendCommand::handleInvoke(const QStringList& args, const KWArgs&)
{
  if (win->sendCommandToProfile(args[0], args[1])) {
    return CommandResult::success();
  }
  return CommandResult::fail();
}
