#ifndef GALOSH_USERPROFILE_H
#define GALOSH_USERPROFILE_H

#include <QMap>
#include <QStringList>
#include <QFont>
#include "triggermanager.h"
#include "explorehistory.h"
#include "colorschemes.h"
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

private:
  bool isGlobalFont;
  bool loadError;

  void saveProfileSection(QSettings& settings);
  void saveAppearanceSection(QSettings& settings);
};

#endif
