#include "serverprofile.h"
#include <QDir>
#include <QStandardPaths>

ServerProfile::ServerProfile(const QString& host)
: host(host)
{
  QString mapFileName, itemsFileName;
  if (host.endsWith(".galosh")) {
    mapFileName = host + "_map";
    itemsFileName = host + "_items";
  } else {
    QDir dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    mapFileName = dir.absoluteFilePath(host + ".galosh_map");
    itemsFileName = dir.absoluteFilePath(host + ".galosh_items");
  }
  map.loadMap(mapFileName);
  itemDB.load(itemsFileName);
}
