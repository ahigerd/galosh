#ifndef GALOSH_MUDLETIMPORT_H
#define GALOSH_MUDLETIMPORT_H

#include "mapmanager.h"
#include <QDataStream>
class QIODevice;

class MudletImport
{
public:
  MudletImport(MapManager* map, QIODevice* dev);

  inline QString importError() const { return err; }

private:
  bool readBinary();

  template <typename FIRST, typename... REST>
  void skip(bool condition = true, const char* /* label */ = nullptr);

  template <typename FIRST, typename... REST>
  void skip(const char* /* label */) { skip<FIRST, REST...>(true); }

  template <typename T>
  void skipLengthPrefixed(const char* /* label */ = nullptr) {
    int count;
    ds >> count;
    while (count-- > 0 && !ds.atEnd()) skip<T>(true);
  }

  MapManager* map;
  QString err;
  QDataStream ds;
  int version;
};

#endif
