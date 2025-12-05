#ifndef GALOSH_USERPROFILE_H
#define GALOSH_USERPROFILE_H

#include <QMap>
#include <QStringList>
#include <QFont>
#include "triggermanager.h"
#include "explorehistory.h"
class ServerProfile;

class UserProfile
{
public:
  UserProfile(const QString& profilePath);

  const QString profilePath;
  QString profileName;
  QString host;
  quint16 port;
  bool tls;
  QString command;
  QString username;
  QString password;
  QString colorScheme;
  QFont font;
  int lastRoomId;

  ServerProfile* serverProfile;
  TriggerManager triggers;

  void setLastRoomId(int roomId);

  void reload();
  // bool save();
};

#endif
