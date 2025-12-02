#include "galoshsession.h"
#include "serverprofile.h"
#include "telnetsocket.h"
#include "infomodel.h"
#include "exploredialog.h"
#include "roomview.h"
#include "commands/textcommand.h"
#include "commands/identifycommand.h"
#include "commands/slotcommand.h"
#include "commands/routecommand.h"
#include "commands/waypointcommand.h"
#include <QSettings>

GaloshSession::GaloshSession(UserProfile* profile, QWidget* parent)
: QObject(parent), TextCommandProcessor("/"), profile(profile), autoMap(map()), exploreHistory(map()), unread(false)
{
  term = new GaloshTerm(parent);
  QObject::connect(term, SIGNAL(lineReceived(QString)), &autoMap, SLOT(processLine(QString)));
  QObject::connect(term, SIGNAL(lineReceived(QString)), itemDB(), SLOT(processLine(QString)));
  QObject::connect(term, SIGNAL(lineReceived(QString)), triggers(), SLOT(processLine(QString)), Qt::QueuedConnection);
  QObject::connect(term, SIGNAL(lineReceived(QString)), this, SLOT(setUnread()));
  QObject::connect(term, SIGNAL(commandEntered(QString, bool)), &autoMap, SLOT(commandEntered(QString, bool)), Qt::QueuedConnection);
  QObject::connect(term, SIGNAL(speedwalk(QStringList)), this, SLOT(speedwalk(QStringList)));

  QObject::connect(term->socket(), SIGNAL(connected()), this, SIGNAL(statusUpdated()));
  QObject::connect(term->socket(), SIGNAL(disconnected()), this, SIGNAL(statusUpdated()));
  QObject::connect(term->socket(), SIGNAL(promptWaiting()), &autoMap, SLOT(promptWaiting()));
  QObject::connect(term->socket(), SIGNAL(msspEvent(QString, QString)), this, SIGNAL(msspReceived()));
  QObject::connect(term->socket(), SIGNAL(gmcpEvent(QString, QVariant)), &autoMap, SLOT(gmcpEvent(QString, QVariant)));
  QObject::connect(term->socket(), SIGNAL(gmcpEvent(QString, QVariant)), this, SLOT(gmcpEvent(QString, QVariant)));

  infoModel = new InfoModel(this);

  addCommand(new IdentifyCommand(&profile->serverProfile->itemDB));
  addCommand(new SlotCommand(".", this, SLOT(abortSpeedwalk()), "Aborts a speedwalk path in progress"));
  addCommand(new SlotCommand("DC", term->socket(), SLOT(disconnectFromHost()), "Disconnects from the game"))->addKeyword("DISCONNECT");
  addCommand(new SlotCommand("EXPLORE", this, SLOT(exploreMap()), "Opens the map exploration window"))->addKeyword("MAP");

  RouteCommand* routeCommand = addCommand(new RouteCommand(map(), &exploreHistory));
  QObject::connect(routeCommand, SIGNAL(speedwalk(QStringList)), this, SLOT(speedwalk(QStringList)));

  WaypointCommand* waypointCommand = addCommand(new WaypointCommand(map(), &exploreHistory));
  QObject::connect(waypointCommand, SIGNAL(speedwalk(QStringList)), this, SLOT(speedwalk(QStringList)));

  QObject::connect(term, SIGNAL(slashCommand(QString, QStringList)), this, SLOT(handleCommand(QString, QStringList)));
  QObject::connect(triggers(), SIGNAL(executeCommand(QString, bool)), term, SLOT(processCommand(QString, bool)), Qt::QueuedConnection);

  QObject::connect(&autoMap, SIGNAL(currentRoomUpdated(int)), this, SLOT(setLastRoom(int)));

  term->installEventFilter(this);
}

GaloshSession::~GaloshSession()
{
  if (term) {
    delete term;
  }
  if (explore) {
    delete explore;
  }
}

QString GaloshSession::name() const
{
  if (profile->profileName.isEmpty()) {
    return "[Unknown profile]";
  }
  return profile->profileName;
}

MapManager* GaloshSession::map() const
{
  return &profile->serverProfile->map;
}

ItemDatabase* GaloshSession::itemDB() const
{
  return &profile->serverProfile->itemDB;
}

TriggerManager* GaloshSession::triggers() const
{
  return &profile->triggers;
}

QHash<QString, QString> GaloshSession::msspData() const
{
  if (!term->socket()) {
    return {};
  }
  return term->socket()->mssp;
}

const MapRoom* GaloshSession::currentRoom() const
{
  return exploreHistory.currentRoom();
}

QString GaloshSession::connectedHost() const
{
  if (!term->socket() || !term->socket()->isConnected()) {
    return QString();
  }
  return term->socket()->hostname();
}

QString GaloshSession::statusBarText() const
{
  QString host = connectedHost();
  if (host.isEmpty()) {
    return "Disconnected.";
  }
  if (statusBar.isEmpty()) {
    return QStringLiteral("Connected to %1.").arg(host);
  }
  return statusBar;
}

void GaloshSession::connect()
{
  if (profile->command.isEmpty()) {
    term->socket()->connectToHost(profile->host, profile->port);
  } else {
    term->socket()->connectCommand(profile->command);
  }
  reload();
}

void GaloshSession::startOffline()
{
  if (profile->command.isEmpty()) {
    term->socket()->setHost(profile->host, profile->port);
  } else {
    term->socket()->setCommand(profile->command);
  }
  setLastRoom(profile->lastRoomId);
  //roomView->setRoom(&map, settings.value("lastRoom", -1).toInt());
}

void GaloshSession::reload()
{
  profile->reload();
  QSettings globalSettings;

  QString colors = profile->colorScheme.isEmpty() ? ColorSchemes::defaultScheme() : profile->colorScheme;
  term->setColorScheme(ColorSchemes::scheme(colors));
  term->setTermFont(profile->font);
}

void GaloshSession::gmcpEvent(const QString& key, const QVariant& value)
{
  if (key.toUpper() == "CHAR") {
    infoModel->loadTree(value);
    emit statusUpdated();
  } else if (key.toUpper() == "EXTERNAL.DISCORD.STATUS") {
    QVariantMap map = value.toMap();
    QString state = map["state"].toString();
    QString details = map["details"].toString();
    QStringList parts;
    if (!state.isEmpty()) {
      parts << state;
    }
    if (!details.isEmpty()) {
      parts << details;
    }
    statusBar = parts.join(" - ");
    emit statusUpdated();
  } else if (key.toUpper() != "ROOM") {
    qDebug() << key << value;
  }
}

void GaloshSession::speedwalk(const QStringList& steps)
{
  if (!speedPath.isEmpty()) {
    term->showError("Another speedwalk is in progress.");
    return;
  }
  term->writeColorLine("96", "Starting speedwalk...");
  speedPath = steps;
  term->processCommand(speedPath.takeFirst());
}

void GaloshSession::abortSpeedwalk()
{
  if (speedPath.isEmpty()) {
    term->showError("No speedwalk to abort.");
  } else {
    speedPath.clear();
    term->writeColorLine("96", "Speedwalk aborted.");
  }
}

void GaloshSession::setLastRoom(int roomId)
{
  profile->setLastRoomId(roomId);
  exploreHistory.goTo(roomId);
  if (!speedPath.isEmpty()) {
    QString dir = speedPath.takeFirst();
    if (dir == "done") {
      term->writeColorLine("93", "Speedwalk complete.");
      return;
    }
    term->processCommand(dir);
    if (speedPath.isEmpty()) {
      speedPath << "done";
    }
  }
  emit currentRoomUpdated();
}

void GaloshSession::showCommandMessage(TextCommand* command, const QString& message, bool isError)
{
  QString formatted = QString(message).replace("\n", "\r\n").replace("\r\r", "\r");
  if (isError) {
    if (command) {
      term->showError(command->name() + ": " + formatted);
    } else {
      term->showError(formatted);
    }
  } else {
    term->writeColorLine("96", formatted.toUtf8());
  }
  setUnread();
}

void GaloshSession::exploreMap(int roomId, const QString& movement)
{
  if (roomId < 0) {
    const MapRoom* room = currentRoom();
    if (room) {
      roomId = room->id;
    }
  }

  if (explore) {
    explore->roomView()->setRoom(map(), roomId, movement);
    explore->refocus();
  } else {
    const MapRoom* prev = exploreHistory.previousRoom();
    explore = new ExploreDialog(this, roomId, prev ? prev->id : -1, movement, term);
    QObject::connect(explore, SIGNAL(exploreRoom(int, QString)), this, SLOT(exploreMap(int, QString)));
    QObject::connect(explore, SIGNAL(openProfileDialog(ProfileDialog::Tab)), this, SIGNAL(openProfileDialog(ProfileDialog::Tab)));
    explore->show();
  }
}

void GaloshSession::setUnread()
{
  bool newUnread = !term->isVisible();
  if (unread != newUnread) {
    unread = newUnread;
    emit unreadUpdated();
  }
}

bool GaloshSession::hasUnread() const
{
  return unread;
}

bool GaloshSession::eventFilter(QObject* obj, QEvent* event)
{
  if (obj == term && event->type() == QEvent::Show)
  {
    setUnread();
  }
  return false;
}
