#include "serverprofile.h"
#include <QSettings>
#include <QDir>
#include <QStandardPaths>
#include <QtDebug>

static QString cleanHost(const QString& host)
{
  if (host.endsWith(".galosh")) {
    QSettings userProfile(host, QSettings::IniFormat);
    userProfile.beginGroup("Profile");
    QString command = userProfile.value("commandLine").toString();
    if (!command.isEmpty()) {
      return command.split(' ').first();
    }
    return userProfile.value("Profile/hostname").toString();
  }
  return host;
}

ServerProfile::ServerProfile(const QString& rawHost)
: host(cleanHost(rawHost))
{
  QString mapFileName, itemsFileName;
  if (rawHost.endsWith(".galosh")) {
    mapFileName = rawHost + "_map";
    itemsFileName = rawHost + "_items";
  } else {
    QDir dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    mapFileName = dir.absoluteFilePath(rawHost + ".galosh_map");
    itemsFileName = dir.absoluteFilePath(rawHost + ".galosh_items");
  }
  map.loadMap(mapFileName);
  itemDB.load(itemsFileName);

  QSettings settings;
  settings.beginGroup("Certificates");
  certificateHash = settings.value(host).toString();
}

void ServerProfile::save()
{
  QSettings settings;
  settings.beginGroup("Certificates");
  if (certificateHash.isEmpty()) {
    settings.remove(host);
  } else {
    settings.setValue(host, certificateHash);
  }
}
