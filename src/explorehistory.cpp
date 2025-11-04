#include "explorehistory.h"
#include "mapmanager.h"
#include "roomview.h"
#include <algorithm>
#include <QtDebug>

ExploreHistory::ExploreHistory(MapManager* map, QObject* parent)
: QObject(parent), map(map), currentRoomId(-1)
{
  // initializers only
}

bool ExploreHistory::canGoBack() const
{
  return steps.length() > 1;
}

QPair<int, int> ExploreHistory::getBounds(int length, bool reverse) const
{
  int from = 0;
  int to = steps.length() - 1;
  if (length > 0) {
    from = steps.length() - length;
    if (from < 0) {
      from = 0;
    }
  }
  if (reverse) {
    std::swap(from, to);
  }
  return qMakePair(from, to);
}

QString ExploreHistory::getReverse(int from, int to, const QString& forward, bool* isGuess) const
{
  const MapRoom* room = map->room(to);
  QString dir;
  if (room) {
    dir = room->findExit(from);
  }
  if (isGuess) {
    *isGuess = dir.isEmpty();
  }
  if (dir.isEmpty()) {
    return MapRoom::reverseDir(forward);
  }
  return dir;
}

QStringList ExploreHistory::describe(int length, bool reverse) const
{
  auto [from, to] = getBounds(length, reverse);
  int inc = reverse ? -1 : 1;
  QString pattern = reverse ? "from %2, go %1" : "%1 to %2";
  QStringList messages;
  for (int i = from; i != to + inc; i += inc) {
    auto [start, dest, dir] = steps[i];
    const MapRoom* room = map->room(dest);
    bool isGuess = false;
    if (reverse) {
      if (i == to) {
        dir.clear();
      } else {
        dir = getReverse(start, dest, dir, &isGuess);
        if (isGuess) {
          dir = "???";
        }
      }
    }
    if (dir.isEmpty()) {
      messages << RoomView::formatRoomTitle(room);
    } else {
      messages << pattern.arg(dir).arg(RoomView::formatRoomTitle(room));
    }
  }
  return messages;
}

QString ExploreHistory::speedwalk(int length, bool reverse, QStringList* warnings) const
{
  auto [from, to] = getBounds(length, false);
  int inc = 1;
  if (length <= 0) {
    for (int i = to; i >= from; i--) {
      if (steps[i].start <= 0 || steps[i].dir.isEmpty()) {
        from = i + 1;
        break;
      }
    }
  }
  if (reverse) {
    inc = -1;
    std::swap(from, to);
  }
  QList<QPair<QString, int>> dirs;
  for (int i = from; i != to + inc; i += inc) {
    const Step& step = steps[i];
    QString dir = step.dir;
    if (step.start <= 0) {
      if (warnings) {
        *warnings << "Warning: path reset during history";
      }
      dir.clear();
    } else if (dir.isEmpty() && reverse) {
      if (warnings) {
        *warnings << "Warning: cannot reverse through auto-move";
      }
    } else if (reverse) {
      bool isGuess = false;
      dir = getReverse(step.dest, step.start, step.dir, &isGuess);
      if (isGuess && warnings) {
        *warnings << QStringLiteral("Warning: uncertain return to %1 from %2 (from %3)").arg(step.start).arg(step.dest).arg(step.dir);
      }
    }
    if (!dirs.isEmpty() && dirs.back().first == dir) {
      dirs.back().second++;
    } else {
      dirs << qMakePair(dir, 1);
    }
  }

  // TODO: customizable speedwalk prefix
  QString msg = ".";
  for (const auto& hist : dirs) {
    if (hist.second > 1) {
      msg += QString::number(hist.second);
    }
    if (hist.first.length() == 1) {
      msg += hist.first;
    } else {
      msg += QStringLiteral("[%1]").arg(hist.first);
    }
  }
  return msg.toLower();
}

void ExploreHistory::reset()
{
  steps.clear();
}

void ExploreHistory::goTo(int roomId)
{
  steps << (Step){ -1, roomId, QString() };
  currentRoomId = roomId;
}

int ExploreHistory::travel(const QString& dir, int dest)
{
  if (!dest) {
    dest = -1;
    const MapRoom* room = map->room(currentRoomId);
    if (room && room->exits.contains(dir)) {
      dest = room->exits.value(dir).dest;
    }
    if (dest <= 0) {
      // can't travel to an unknown destination
      return -1;
    }
  }
  if (steps.length() && (dir.isEmpty() || dir == steps.last().dir)) {
    if (steps.last().dest == dest) {
      // ignore no-op travel into the same room
      return dest;
    }
  }
  steps << (Step){ currentRoomId, dest, dir };
  currentRoomId = dest;
  return dest;
}

int ExploreHistory::back()
{
  if (!canGoBack()) {
    return -1;
  }
  steps.takeLast();
  currentRoomId = steps.last().dest;
  return currentRoomId;
}

void ExploreHistory::simplify(bool aggressive)
{
  QSet<int> visited;
  QMap<int, int> firstAvailable;
  QList<Step> simplified;
  for (const Step& step : steps) {
    const MapRoom* room = map->room(step.dest);
    if (room && aggressive) {
      for (const MapExit& exit : room->exits) {
        if (exit.dest > 0 && !firstAvailable.contains(exit.dest)) {
          firstAvailable[exit.dest] = step.dest;
        }
      }
      const MapRoom* shortcut = map->room(firstAvailable.value(step.dest, -1));
      if (shortcut && shortcut->id != step.start && visited.contains(shortcut->id)) {
        QString shortcutDir = shortcut->findExit(step.dest);
        if (shortcutDir.isEmpty()) {
          qWarning() << "XXX: impossible state detected";
        }
        for (auto iter = simplified.rbegin(); iter != simplified.rend(); iter++) {
          visited.remove(iter->dest);
          if (iter->start == shortcut->id) {
            simplified.erase(iter.base(), simplified.end());
            iter->dest = step.dest;
            iter->dir = shortcutDir;
            break;
          }
        }
        continue;
      }
    }
    if (visited.contains(step.dest)) {
      for (auto iter = simplified.rbegin(); iter != simplified.rend(); iter++) {
        if (iter->dest == step.dest) {
          simplified.erase(iter.base(), simplified.end());
          break;
        }
        visited.remove(iter->dest);
      }
    } else {
      visited << step.dest;
      simplified << step;
    }
  }
  steps = simplified;
}

const MapRoom* ExploreHistory::currentRoom() const
{
  return map->room(currentRoomId);
}
