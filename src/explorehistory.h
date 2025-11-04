#ifndef EXPLOREHISTORY_H
#define EXPLOREHISTORY_H

#include <QObject>
#include <QStringList>
class MapManager;

class ExploreHistory : public QObject
{
Q_OBJECT
public:
  ExploreHistory(MapManager* map, QObject* parent = nullptr);

  QStringList describe(int length, bool reverse = false) const;
  QString speedwalk(int length, bool reverse = false, QStringList* warnings = nullptr) const;

  bool canGoBack() const;
  inline int length() const { return steps.length(); }

public slots:
  void reset();
  void goTo(int roomId);
  int travel(const QString& dir, int dest = 0);
  int back();
  void simplify();

private:
  QString getReverse(int from, int to, const QString& forward, bool* isGuess) const;
  QPair<int, int> getBounds(int length, bool reverse) const;

  struct Step {
    int start; // -1 = reset
    int dest;
    QString dir; // empty = reset / auto-move
  };
  void debug(const QList<Step>& path) const;
  MapManager* map;
  QList<Step> steps;
  int currentRoomId;
};

#endif
