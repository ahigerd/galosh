#include "exploredialog.h"
#include "galoshsession.h"
#include "mapmanager.h"
#include "mapzone.h"
#include "roomview.h"
#include "mapviewer.h"
#include "mapoptions.h"
#include "commandline.h"
#include "waypointstab.h"
#include "commands/slotcommand.h"
#include "commands/mapsearchcommand.h"
#include "commands/maphistorycommand.h"
#include "commands/routecommand.h"
#include "commands/simplifycommand.h"
#include "commands/waypointcommand.h"
#include "commands/zonecommand.h"
#include <QSettings>
#include <QGridLayout>
#include <QMenuBar>
#include <QToolButton>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QSplitter>
#include <QDialogButtonBox>
#include <QMetaObject>
#include <algorithm>

ExploreDialog::ExploreDialog(GaloshSession* session, int roomId, int lastRoomId, const QString& movement, QWidget* parent)
: QMainWindow(parent), session(session), map(session->map()), history(map)
{
  setAttribute(Qt::WA_WindowPropagation, true);
  setAttribute(Qt::WA_DeleteOnClose, true);

  setWindowTitle("Map Explorer: " + session->name());

  QWidget* widget = new QWidget(this);
  setCentralWidget(widget);

  QVBoxLayout* vbox = new QVBoxLayout(widget);
  vbox->setContentsMargins(0, 0, 0, 0);

  splitter = new QSplitter(Qt::Vertical, widget);
  QObject::connect(splitter, SIGNAL(splitterMoved(int,int)), this, SLOT(saveState()));
  vbox->addWidget(splitter, 1);

  mapView = new MapViewer(MapViewer::EmbedMap, widget);
  mapView->setSession(session);
  QObject::connect(mapView, SIGNAL(exploreMap(int)), this, SIGNAL(exploreRoom(int)));
  splitter->addWidget(mapView);

  QWidget* lowerPane = new QWidget(splitter);
  splitter->addWidget(lowerPane);

  QGridLayout* layout = new QGridLayout(lowerPane);
  QMargins margins = layout->contentsMargins();
  margins.setTop(margins.top() + 2);
  layout->setContentsMargins(margins);

  roomTitle = new QLabel(lowerPane);
  layout->addWidget(roomTitle, 0, 0, 1, 2);

  room = new RoomView(map, roomId, lastRoomId, lowerPane);
  room->layout()->setContentsMargins(1, 0, 0, 0);
  layout->addWidget(room, 1, 0, 1, 2);

  line = new CommandLine(lowerPane);
  line->setParsing(false); // we'll handle the parsing here
  layout->addWidget(line, 2, 0);

  backButton = new QPushButton("&Back", lowerPane);
  backButton->setDefault(false);
  backButton->setAutoDefault(false);
  layout->addWidget(backButton, 2, 1);

  layout->setRowStretch(0, 0);
  layout->setRowStretch(1, 1);
  layout->setRowStretch(2, 0);
  layout->setColumnStretch(0, 1);
  layout->setColumnStretch(1, 0);

  QMenuBar* mb = new QMenuBar(this);
  setMenuBar(mb);

  QMenu* mMap = new QMenu("&Map", mb);
  mMap->addAction("&Waypoints...", this, SLOT(openWaypoints()));
  mMap->addAction("&Settings...", this, SLOT(openMapOptions()));
  mMap->addSeparator();
  mMap->addAction("&Close", this, SLOT(close()), QKeySequence::Close);
  mb->addMenu(mMap);

  QMenu* mView = new QMenu("&View", mb);
  mView->addAction("Zoom &In", mapView, SLOT(zoomIn()))->setShortcuts({ QKeySequence::ZoomIn, QKeySequence("Ctrl+=") });
  mView->addAction("Zoom &Out", mapView, SLOT(zoomOut()))->setShortcuts(QKeySequence::ZoomOut);
  mView->addSeparator();
  pinAction = mView->addAction("Always on &Top", this, SLOT(togglePin()));
  pinAction->setCheckable(true);
  mb->addMenu(mView);

  QMenu* mHelp = new QMenu("&Help", mb);
  mHelp->addAction("&Help", this, SLOT(showHelp()));
  mb->addMenu(mHelp);

  QObject::connect(room, SIGNAL(roomUpdated(QString, int, QString)), this, SLOT(roomUpdated(QString, int, QString)));
  QObject::connect(room, SIGNAL(exploreRoom(int, QString)), this, SIGNAL(exploreRoom(int, QString)));
  QObject::connect(backButton, SIGNAL(clicked()), this, SLOT(goBack()));
  QObject::connect(line, SIGNAL(returnPressed()), this, SLOT(doCommand()));
  QObject::connect(this, SIGNAL(exploreRoom(int, QString)), mapView, SLOT(setCurrentRoom(int)));

  const MapRoom* room = nullptr;
  if (lastRoomId != -1) {
    history.goTo(lastRoomId);
    room = map->room(lastRoomId);
  }
  if (roomId != -1) {
    history.travel(movement, roomId);
    room = map->room(lastRoomId);
  }
  if (room) {
    roomTitle->setText(room->name);
  } else {
    roomTitle->setText("Unknown location");
  }

  line->setFocus();

  addCommand(new MapSearchCommand(map));
  addCommand(new ZoneCommand(map));
  addCommand(new MapHistoryCommand("HISTORY", &history));
  addCommand(new MapHistoryCommand("REVERSE", &history));
  addCommand(new MapHistoryCommand("SPEED", &history));
  addCommand(new SimplifyCommand(&history));
  addCommand(new SlotCommand("BACK", this, SLOT(goBack()), "Returns to the previous room"));
  addCommand(new SlotCommand("GOTO", this, SLOT(goToRoom(QString)), "Teleports to a room by numeric ID"));
  addCommand(new SlotCommand("RESET", &history, SLOT(reset()), "Clears the exploration history"));

  RouteCommand* route = addCommand(new RouteCommand(map, &history));
  QObject::connect(route, SIGNAL(speedwalk(QStringList)), this, SLOT(handleSpeedwalk(QStringList)));

  WaypointCommand* waypoint = addCommand(new WaypointCommand(map, &history));
  QObject::connect(waypoint, SIGNAL(speedwalk(QStringList)), this, SLOT(handleSpeedwalk(QStringList)));

  QSettings settings;
  restoreGeometry(settings.value("explore").toByteArray());
  splitter->restoreState(settings.value("exploreSplit").toByteArray());
  pinAction->setChecked(settings.value("explorePinned").toBool());
  setWindowFlag(Qt::WindowStaysOnTopHint, pinAction->isChecked());
}

void ExploreDialog::roomUpdated(const QString& title, int id, const QString& movement)
{
  roomTitle->setText(title);
  history.travel(movement, id);
  backButton->setEnabled(history.canGoBack());
}

void ExploreDialog::doCommand()
{
  responseLines.clear();
  responseError = false;
  expectResponse = true;
  clearResponse();
  QString command = line->text();
  if (command.startsWith("/")) {
    // politely ignore slashes
    command.remove(0, 1);
  }
  handleCommand(command);
  if (expectResponse) {
    setResponse(responseError, responseLines.join("\n"));
  }
  line->selectAll();
}

void ExploreDialog::handleSpeedwalk(const QStringList& dirs)
{
  if (dirs.isEmpty()) {
    setResponse(true);
    return;
  }
  QStringList messages;
  bool error = false;
  QString lastRoom;
  int cost = 0;
  const MapRoom* room = nullptr;
  for (int i = 0; i < dirs.length(); i++) {
    const MapRoom* room = history.currentRoom();
    cost += room ? map->roomCost(room->roomType) : 1;
    QString dir = MapRoom::normalizeDir(dirs[i]);
    int dest = history.travel(dir);
    if (dest < 0) {
      error = true;
      messages << QStringLiteral("Unable to travel %1 from this location").arg(dir);
      if (i + 1 < dirs.length()) {
        messages << QStringLiteral("Remaining steps: %1").arg(dirs.mid(i + 1).join(" "));
      }
      break;
    }
    lastRoom = RoomView::formatRoomTitle(history.currentRoom());
    messages << QStringLiteral("%1 to %2").arg(dir).arg(lastRoom);
  }
  messages << "" << QStringLiteral("Total movement cost: %1").arg(cost);
  room = history.currentRoom();
  roomView()->setRoom(map, room ? room->id : -1);
  roomTitle->setText(room ? room->name : "Unknown location");
  setResponse(error, messages.join("\n"));
}

bool ExploreDialog::commandFilter(const QString& command, const QStringList& args)
{
  if (command.startsWith(".")) {
    handleSpeedwalk(CommandLine::parseSpeedwalk(command + args.join("")));
    return true;
  }

  QString dir = MapRoom::normalizeDir(command);
  if (MapRoom::isDir(dir)) {
    int dest = history.travel(dir);
    if (dest < 0) {
      responseError = true;
    } else {
      emit exploreRoom(dest, dir);
    }
    return true;
  }

  return false;
}

void ExploreDialog::showCommandMessage(TextCommand* command, const QString& message, bool isError)
{
  if (command && isError) {
    responseLines << QString("%1: %2").arg(command->name(), message);
  } else {
    responseLines << message;
  }
  responseError = responseError || isError;
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
  expectResponse = false;
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

void ExploreDialog::goBack()
{
  if (!history.canGoBack()) {
    setResponse(true, "No room to go back to.");
    return;
  }
  int roomId = history.back();
  const MapRoom* room = map->room(roomId);
  if (!room) {
    setResponse(true, "Could not load destination room.");
    return;
  }
  emit exploreRoom(roomId, QString());
  clearResponse();
}

void ExploreDialog::goToRoom(const QString& id)
{
  int roomId = id.toInt();
  if (roomId <= 0) {
    roomId = map->waypoint(id);
    if (roomId <= 0) {
      setResponse(true, "Invalid room ID: " + id);
      return;
    }
  }
  emit exploreRoom(roomId, QString());
}

void ExploreDialog::togglePin()
{
  setWindowFlag(Qt::WindowStaysOnTopHint, pinAction->isChecked());
  show();
  saveState();
}

void ExploreDialog::showHelp()
{
  responseLines.clear();
  help();
  setResponse(false, responseLines.join("\n"));
}

void ExploreDialog::saveState()
{
  QSettings settings;
  settings.setValue("explore", saveGeometry());
  settings.setValue("exploreSplit", splitter->saveState());
  settings.setValue("explorePinned", pinAction->isChecked());
}

void ExploreDialog::resizeEvent(QResizeEvent*)
{
  saveState();
}

void ExploreDialog::moveEvent(QMoveEvent*)
{
  saveState();
}

void ExploreDialog::openMapOptions()
{
  MapOptions* o = new MapOptions(map, this);
  o->open();
}

void ExploreDialog::openWaypoints()
{
  // TODO: this is a quick-and-dirty implementation to take the server-level tab out of the profiles dialog
  QDialog d;
  QVBoxLayout* layout = new QVBoxLayout(&d);

  WaypointsTab* t = new WaypointsTab(&d);
  layout->addWidget(t, 1);

  QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &d);
  layout->addWidget(buttons, 0);
  QObject::connect(buttons, SIGNAL(accepted()), &d, SLOT(accept()));
  QObject::connect(buttons, SIGNAL(rejected()), &d, SLOT(reject()));

  t->load(session->profile->profilePath);
  if (d.exec()) {
    t->save(session->profile->profilePath);
  }
}
