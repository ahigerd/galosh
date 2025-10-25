#include "triggermanager.h"
#include <QSettings>

TriggerManager::TriggerManager(QObject* parent)
: QObject(parent)
{
  // initializers only
}

void TriggerManager::loadProfile(const QString& profile)
{
  triggers.clear();
  QSettings settings(profile, QSettings::IniFormat);
  settings.beginGroup("Profile");

  QString username = settings.value("username").toString();
  QString loginPrompt = settings.value("loginPrompt").toString();
  if (!loginPrompt.isEmpty() && !username.isEmpty()) {
    triggers << (Trigger){ QRegularExpression(loginPrompt), username, true, true, false };
  }

  QString password = settings.value("password").toString();
  QString passwordPrompt = settings.value("passwordPrompt").toString();
  if (!passwordPrompt.isEmpty() && !password.isEmpty()) {
    triggers << (Trigger){ QRegularExpression(passwordPrompt), password, false, true, false };
  }

  triggers << (Trigger){ QRegularExpression("You feel less intelligent."), "innate brill", true, false, false };
}

void TriggerManager::processLine(const QString& line)
{
  for (Trigger& trigger : triggers) {
    if (trigger.once && trigger.triggered) {
      continue;
    }
    auto match = trigger.pattern.match(line);
    if (match.hasMatch()) {
      trigger.triggered = true;
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
