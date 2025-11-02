#include "triggermanager.h"
#include <QSettings>
#include <QSet>

TriggerDefinition::TriggerDefinition(const QString& id)
: id(id), echo(true), enabled(true), once(false), triggered(false)
{
  // initializers only
}

TriggerDefinition::TriggerDefinition(QSettings* profile, const QString& id)
: TriggerDefinition(id)
{
  profile->beginGroup(id);
  pattern.setPattern(profile->value("pattern").toString());
  command = profile->value("command").toString();
  echo = profile->value("echo", true).toBool();
  enabled = profile->value("enabled", true).toBool();
  profile->endGroup();
}

void TriggerDefinition::save(QSettings* profile) const
{
  profile->beginGroup(id);
  profile->setValue("pattern", pattern.pattern());
  profile->setValue("command", command);
  if (echo) {
    profile->remove("echo");
  } else {
    profile->setValue("echo", false);
  }
  if (enabled) {
    profile->remove("enabled");
  } else {
    profile->setValue("enabled", false);
  }
  profile->endGroup();
}

void TriggerDefinition::remove(QSettings* profile) const
{
  profile->remove(id);
}

bool TriggerDefinition::isInternal() const
{
  return id.size() && id[0] == '\x01';
}

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
    TriggerDefinition def("\x01username");
    def.pattern.setPattern(QRegularExpression::escape(loginPrompt));
    def.command = username;
    def.once = true;
    triggers << def;
  }

  QString password = settings.value("password").toString();
  QString passwordPrompt = settings.value("passwordPrompt").toString();
  if (!passwordPrompt.isEmpty() && !password.isEmpty()) {
    TriggerDefinition def("\x01password");
    def.pattern.setPattern(QRegularExpression::escape(passwordPrompt));
    def.command = password;
    def.once = true;
    def.echo = false;
    triggers << def;
  }
  settings.endGroup();

  settings.beginGroup("Triggers");
  for (const QString& key : settings.childGroups()) {
    TriggerDefinition def(&settings, key);
    triggers << def;
  }
}

void TriggerManager::saveProfile(const QString& profile)
{
  QSettings settings(profile, QSettings::IniFormat);
  settings.beginGroup("Triggers");
  QSet<QString> knownIds;
  for (const auto& def : triggers) {
    if (def.isInternal()) {
      continue;
    }
    knownIds << def.id;
    def.save(&settings);
  }
  for (const QString& key : settings.childGroups()) {
    if (knownIds.contains(key)) {
      continue;
    }
    settings.remove(key);
  }
}

void TriggerManager::processLine(const QString& line)
{
  for (TriggerDefinition& trigger : triggers) {
    if (!trigger.enabled || (trigger.once && trigger.triggered)) {
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

TriggerDefinition* TriggerManager::findTrigger(const QString& id)
{
  for (TriggerDefinition& def : triggers) {
    if (def.id == id) {
      return &def;
    }
  }
  return nullptr;
}
