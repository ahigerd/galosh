#ifndef GALOSH_SERVERPROFILE_H
#define GALOSH_SERVERPROFILE_H

#include "mapmanager.h"
#include "itemdatabase.h"

class ServerProfile
{
public:
  ServerProfile(const QString& host);

  const QString host;
  MapManager map;
  ItemDatabase itemDB;
};

#endif
