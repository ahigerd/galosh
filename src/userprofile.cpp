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
  serverProfile = getServerProfile(host.isEmpty() ? profilePath : host);
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

  {
    SettingsGroup sg(&settings, "Profile");
    profileName = settings.value("name").toString();
    host = settings.value("host").toString();
    port = settings.value("port").value<quint16>();
    tls = settings.value("tls").toBool();
    command = settings.value("commandLine").toString();
    lastRoomId = settings.value("lastRoom", -1).toInt();
  }

  {
    SettingsGroup sg(&settings, "Appearance");
    colorScheme = settings.value("colors").toString();
    if (globalSettings.value("Defaults/fontGlobal").toBool() || !settings.contains("font")) {
      font = globalSettings.value("Defaults/font", QFontDatabase::systemFont(QFontDatabase::FixedFont)).value<QFont>();
    } else {
      font = settings.value("font").value<QFont>();
    }
  }

  triggers.loadProfile(profilePath);
}

/*
bool UserProfile::save()
{
  // TODO: refactor to use this instead of having the profile dialog do it
}
*/
