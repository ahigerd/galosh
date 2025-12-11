#include "userprofile.h"
#include "serverprofile.h"
#include "settingsgroup.h"
#include <QFontDatabase>
#include <QSettings>
#include <QtDebug>

static ServerProfile* getServerProfile(const QString& host)
{
  static QMap<QString, ServerProfile*> serversByHost;
  ServerProfile* server = serversByHost.value(host);
  if (!server) {
    server = new ServerProfile(host);
    serversByHost[host] = server;
  }
  return server;
}

UserProfile::UserProfile(const QString& profilePath)
: profilePath(profilePath)
{
  reload();
}

bool UserProfile::hasLoadError() const
{
  return loadError;
}

bool UserProfile::useGlobalFont()
{
  QSettings globalSettings;
  return globalSettings.value("Defaults/fontGlobal").toBool();
}

QFont UserProfile::font() const
{
  QSettings globalSettings;
  if (!setAsGlobalFont && globalSettings.value("Defaults/fontGlobal").toBool()) {
    return globalSettings.value("Defaults/font", QFontDatabase::systemFont(QFontDatabase::FixedFont)).value<QFont>();
  } else {
    return selectedFont;
  }
}

ColorScheme UserProfile::colors() const
{
  return ColorSchemes::scheme(colorScheme);
}

void UserProfile::setLastRoomId(int roomId)
{
  lastRoomId = roomId;
  QSettings settings(profilePath, QSettings::IniFormat);
  SettingsGroup sg(&settings, "Profile");
  settings.setValue("lastRoom", roomId);
}

void UserProfile::reload()
{
  QSettings globalSettings;
  QSettings settings(profilePath, QSettings::IniFormat);
  loadError = (settings.status() != QSettings::NoError);
  if (loadError) {
    qWarning() << "XXX: loadError" << settings.status();
    return;
  }

  {
    SettingsGroup sg(&settings, "Profile");
    profileName = settings.value("name").toString();
    host = settings.value("host").toString();
    port = settings.value("port").value<quint16>();
    tls = settings.value("tls").toBool();
    command = settings.value("commandLine").toString();
    username = settings.value("username").toString();
    password = settings.value("password").toString();
    lastRoomId = settings.value("lastRoom", -1).toInt();
  }

  {
    SettingsGroup sg(&settings, "Appearance");
    colorScheme = settings.value("colors").toString();
    selectedFont = settings.value("font", QFontDatabase::systemFont(QFontDatabase::FixedFont)).value<QFont>();
  }

  triggers.loadProfile(profilePath);

  serverProfile = getServerProfile(host.isEmpty() ? profilePath : host);
}

bool UserProfile::save()
{
  QSettings settings(profilePath, QSettings::IniFormat);
  if (!settings.isWritable() || settings.status() != QSettings::NoError) {
    qWarning() << "XXX: save error" << settings.status();
    return false;
  }
  saveProfileSection(settings);
  saveAppearanceSection(settings);


  settings.sync();
  return settings.status() != QSettings::NoError;
}

void UserProfile::saveProfileSection(QSettings& settings)
{
  SettingsGroup sg(&settings, "Profile");
  settings.setValue("name", profileName);
  if (command.isEmpty()) {
    settings.setValue("host", host);
    settings.setValue("port", port);
    if (tls) {
      settings.setValue("tls", true);
    } else {
      settings.remove("tls");
    }
    settings.remove("commandLine");
  } else {
    settings.remove("host");
    settings.remove("port");
    settings.remove("tls");
    settings.setValue("commandLine", command);
  }
  settings.setValue("username", username);
  settings.setValue("password", password);

  TriggerDefinition* usernameTrigger = triggers.findTrigger(TriggerManager::UsernameId);
  if (usernameTrigger) {
    settings.setValue("loginPrompt", usernameTrigger->pattern.pattern());
  } else {
    settings.remove("loginPrompt");
  }

  TriggerDefinition* passwordTrigger = triggers.findTrigger(TriggerManager::PasswordId);
  if (passwordTrigger) {
    settings.setValue("passwordPrompt", passwordTrigger->pattern.pattern());
  } else {
    settings.remove("passwordPrompt");
  }
}


void UserProfile::saveAppearanceSection(QSettings& settings)
{
  SettingsGroup sg(&settings, "Appearance");
  if (colorScheme.isEmpty()) {
    settings.remove("colors");
  } else {
    settings.setValue("colors", colorScheme);
  }

  QSettings globalSettings;
  if (setAsGlobalFont) {
    isGlobalFont = true;
    globalSettings.setValue("Defaults/fontGlobal", true);
    globalSettings.setValue("Defaults/font", selectedFont);
    settings.remove("font");
  } else {
    if (isGlobalFont) {
      globalSettings.remove("Defaults/fontGlobal");
      globalSettings.remove("Defaults/font");
      isGlobalFont = false;
    }
    settings.setValue("font", selectedFont);
  }
}
