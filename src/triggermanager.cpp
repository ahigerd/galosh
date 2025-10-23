#include "triggermanager.h"
#include <QSettings>

TriggerManager::TriggerManager(QObject* parent)
: QObject(parent)
{
  triggers << (Trigger){ QRegularExpression("^Password:"), "insanity", false };
  triggers << (Trigger){ QRegularExpression("You feel less intelligent."), "innate brill", true };
}

void TriggerManager::loadProfile(const QString& profile)
{
  Q_UNUSED(profile);
}

void TriggerManager::processLine(const QString& line)
{
  for (const Trigger& trigger : triggers) {
    auto match = trigger.pattern.match(line);
    if (match.hasMatch()) {
      QString command = trigger.command;
      auto groups = match.capturedTexts();
      groups.removeFirst();
      for (const QString& group : groups) {
        command = command.arg(group);
      }
      emit executeCommand(command, trigger.echo);
    }
  }
}
