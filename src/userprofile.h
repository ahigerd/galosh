#ifndef GALOSH_USERPROFILE_H
#define GALOSH_USERPROFILE_H

#include <QMap>
#include <QStringList>
#include <QFont>
#include "triggermanager.h"
#include "explorehistory.h"
#include "colorschemes.h"
#include "itemdatabase.h"
class QSettings;
class ServerProfile;

class UserProfile
{
public:
  UserProfile(const QString& profilePath);

  bool hasLoadError() const;

  const QString profilePath;
  QString profileName;
  QString host;
  quint16 port;
  bool tls;
  QString command;
  QString username;
  QString password;
  int lastRoomId;

  QString colorScheme;
  QFont selectedFont;
  bool setAsGlobalFont;

  ServerProfile* serverProfile;
  TriggerManager triggers;

  QFont font() const;
  static bool useGlobalFont();

  ColorScheme colors() const;

  void setLastRoomId(int roomId);

  void reload();
  bool save();

  QStringList itemSets() const;
  ItemDatabase::EquipmentSet loadItemSet(const QString& name) const;
  void saveItemSet(const QString& name, const ItemDatabase::EquipmentSet& items);
  void removeItemSet(const QString& name);

private:
  bool isGlobalFont;
  bool loadError;

  void saveProfileSection(QSettings& settings);
  void saveAppearanceSection(QSettings& settings);
};

#endif
