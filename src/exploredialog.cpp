#include "exploredialog.h"
#include "mapmanager.h"
#include "roomview.h"
#include <QGridLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QMetaObject>
#include <algorithm>
#include <QtDebug>

ExploreDialog::ExploreDialog(MapManager* map, int roomId, int lastRoomId, const QString& movement, QWidget* parent)
: QDialog(parent), map(map)
{
  setWindowFlag(Qt::Dialog, true);
  setWindowFlag(Qt::WindowStaysOnTopHint, true);
  setAttribute(Qt::WA_WindowPropagation, true);
  setAttribute(Qt::WA_WindowPropagation, true);
  setAttribute(Qt::WA_DeleteOnClose, true);

  QGridLayout* layout = new QGridLayout(this);
  QMargins margins = layout->contentsMargins();
  margins.setTop(margins.top() + 2);
  layout->setContentsMargins(margins);

  room = new RoomView(map, roomId, lastRoomId, this);
  room->layout()->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(room, 0, 0, 1, 2);

  line = new QLineEdit(this);
  layout->addWidget(line, 1, 0);

  backButton = new QPushButton("&Back", this);
  backButton->setDefault(false);
  backButton->setAutoDefault(false);
  layout->addWidget(backButton, 1, 1);

  layout->setRowStretch(0, 1);
  layout->setRowStretch(1, 0);
  layout->setColumnStretch(0, 1);
  layout->setColumnStretch(1, 0);

  QObject::connect(room, SIGNAL(roomUpdated(QString, int, QString)), this, SLOT(roomUpdated(QString, int, QString)));
  QObject::connect(room, SIGNAL(exploreRoom(int, QString)), this, SIGNAL(exploreRoom(int, QString)));
  QObject::connect(backButton, SIGNAL(clicked()), this, SLOT(do_BACK()));
  QObject::connect(line, SIGNAL(returnPressed()), this, SLOT(doCommand()));

  const MapRoom* room = nullptr;
  if (lastRoomId != -1) {
    addHistory(lastRoomId, "");
    room = map->room(lastRoomId);
  }
  if (roomId != -1) {
    addHistory(roomId, movement);
    room = map->room(lastRoomId);
  }
  if (room) {
    setWindowTitle(room->name);
  } else {
    setWindowTitle("Unknown location");
  }

  line->setFocus();
}

void ExploreDialog::roomUpdated(const QString& title, int id, const QString& movement)
{
  setWindowTitle(title);
  addHistory(id, movement);
  backButton->setEnabled(roomHistory.size() > 1);
}

void ExploreDialog::addHistory(int id, const QString& movement)
{
  if (id > 0) {
    QPair<int, QString> back(-1, "");
    if (!roomHistory.isEmpty()) {
      back = roomHistory.back();
    }
    if (back.first != id || (!back.second.isEmpty() && back.second != movement)) {
      roomHistory << qMakePair(id, movement);
    }
  }
}

void ExploreDialog::doCommand()
{
  QString dir = MapRoom::normalizeDir(line->text().simplified());
  if (dir.isEmpty()) {
    return;
  }

  clearResponse();
  if (MapRoom::isDir(dir)) {
    int dest = room->exitDestination(dir);
    if (dest < 0) {
      setResponse(true);
    } else {
      emit exploreRoom(dest, dir);
    }
  } else {
    QStringList args = dir.split(' ');
    QString command = args.takeFirst();
    handleCommand(command, args);
  }
  line->selectAll();
}

void ExploreDialog::handleCommand(const QString& command, const QStringList& args)
{
  QByteArray slot = QStringLiteral("do_%1").arg(command).toUtf8();
  if (metaObject()->indexOfSlot((slot + "(QStringList)").constData()) >= 0) {
    if (!QMetaObject::invokeMethod(this, slot.constData(), Q_ARG(QStringList, args))) {
      setResponse(true, QStringLiteral("Unknown error running command: %1").arg(command));
    }
  } else if (metaObject()->indexOfSlot((slot + "()").constData()) >= 0) {
    if (args.length()) {
      setResponse(true, QStringLiteral("%1 does not accept any parameters").arg(command));
    } else if (!QMetaObject::invokeMethod(this, slot.constData())) {
      setResponse(true, QStringLiteral("Unknown error running command: %1").arg(command));
    }
  } else {
    setResponse(true, QStringLiteral("Unknown command: %1").arg(command));
  }
}

void ExploreDialog::refocus()
{
  show();
  raise();
  setFocus();
  line->setFocus();
  line->selectAll();
}

void ExploreDialog::setResponse(bool isError, const QString& message)
{
  QPalette p = palette();
  if (isError) {
    p.setColor(QPalette::Base, QColor(255, 128, 128));
    QTimer::singleShot(100, this, SLOT(setResponse()));
    line->selectAll();
    line->setFocus();
  }
  line->setPalette(p);
  if (!message.isEmpty() || isError) {
    room->setResponseMessage(message, isError);
  }
}

void ExploreDialog::clearResponse()
{
  room->setResponseMessage("", false);
}

void ExploreDialog::do_BACK()
{
  if (roomHistory.size() < 2) {
    setResponse(true, "No room to go back to.");
    return;
  }
  roomHistory.takeLast();
  auto back = roomHistory.takeLast();
  emit exploreRoom(back.first, back.second);
  clearResponse();
}

void ExploreDialog::do_HELP()
{
}

void ExploreDialog::do_GOTO(const QStringList& args)
{
  // TODO: flesh out
  if (args.length() == 1) {
    int roomId = args[0].toInt();
    emit exploreRoom(roomId, QString());
  }
}

void ExploreDialog::do_ZONE(const QStringList&)
{
}

void ExploreDialog::do_FIND(const QStringList&)
{
}

void ExploreDialog::do_SEARCH(const QStringList&)
{
}

void ExploreDialog::do_HISTORY(const QStringList& args)
{
  int len = -1;
  bool speedwalk = false;
  bool reverse = false;
  QString badArg;
  for (const QString& arg : args) {
    bool isNumeric = false;
    int numeric = arg.toInt(&isNumeric);
    if (isNumeric) {
      if (numeric <= 0 || len > 0) {
        badArg = arg;
        break;
      }
      len = numeric;
    } else if (QStringLiteral("SPEED").startsWith(arg)) {
      speedwalk = true;
    } else if (QStringLiteral("REVERSE").startsWith(arg)) {
      reverse = true;
    } else if (arg == "*" || arg == "ALL") {
      len = roomHistory.length();
    } else {
      badArg = arg;
      break;
    }
  }
  if (!badArg.isEmpty()) {
    setResponse(true, QStringLiteral("Could not understand \"%1\".").arg(badArg));
    return;
  }

  if (len < 1) {
    if (speedwalk) {
      len = roomHistory.length();
    } else {
      len = 10;
    }
  }

  QList<QPair<int, QString>> path = roomHistory.mid(roomHistory.length() - len);
  int prevRoom = -1;
  if (speedwalk) {
    for (int i = path.length() - 1; i >= 0; --i) {
      if (path[i].second.isEmpty() || path[i].first < 0) {
        prevRoom = path[i].first;
        path = path.mid(i);
        break;
      }
    }
  }
  if (path.length() < 1) {
    setResponse(false, "No history to trace.");
    return;
  }
  if (prevRoom < 0) {
    prevRoom = path.first().first;
  }
  QStringList messages;
  if (reverse) {
    QList<QPair<int, QString>> rev;
    int rprev = path.back().first;
    if (!speedwalk) {
      rev << qMakePair(rprev, QString());
    }
    for (int i = path.length() - 2; i >= 0; --i) {
      int roomId = path[i].first;
      const MapRoom* room = map->room(rprev);
      QString revDir = room->findExit(roomId);
      if (revDir.isEmpty()) {
        messages << QStringLiteral("Warning: cannot return to %1 from %2 (from %3)").arg(roomId).arg(rprev).arg(path[i].second);
        rev << qMakePair(roomId, MapRoom::reverseDir(path[i].second));
      } else {
        rev << qMakePair(roomId, revDir);
      }
      rprev = roomId;
    }
    path = rev;
  }
  if (speedwalk) {
    QList<QPair<QString, int>> dirs;
    for (const auto& hist : path) {
      QString dir = hist.second;
      if (!dirs.isEmpty() && dirs.back().first == dir) {
        dirs.back().second++;
      } else {
        dirs << qMakePair(dir, 1);
      }
      prevRoom = hist.first;
    }
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
    messages << msg.toLower();
  } else {
    for (const auto& hist : path) {
      QString dir = hist.second;
      const MapRoom* room = map->room(hist.first);
      if (dir.isEmpty()) {
        messages << RoomView::formatRoomTitle(room);
      } else {
        messages << QStringLiteral("%1 to %2").arg(dir).arg(RoomView::formatRoomTitle(room));
      }
      prevRoom = hist.first;
    }
  }
  setResponse(false, messages.join("\n"));
}

void ExploreDialog::do_REVERSE(const QStringList& args)
{
  do_HISTORY(QStringList(args) << "REVERSE");
}

void ExploreDialog::do_RESET()
{
  roomHistory.clear();
}
