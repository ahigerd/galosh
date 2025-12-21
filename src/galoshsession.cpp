#include "galoshsession.h"
#include "serverprofile.h"
#include "telnetsocket.h"
#include "infomodel.h"
#include "exploredialog.h"
#include "roomview.h"
#include "itemsetdialog.h"
#include "itemsearchdialog.h"
#include "itemsetdialog.h"
#include "commands/textcommand.h"
#include "commands/equipmentcommand.h"
#include "commands/identifycommand.h"
#include "commands/slotcommand.h"
#include "commands/speedwalkcommand.h"
#include "commands/routecommand.h"
#include "commands/waypointcommand.h"
#include "algorithms.h"
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QSettings>

GaloshSession::GaloshSession(UserProfile* profile, QWidget* parent)
: QObject(parent), TextCommandProcessor("/"), profile(profile), autoMap(map()), exploreHistory(map()), unread(false)
{
  term = new GaloshTerm(parent);
  term->setCommandFilter([this](const QString& command) { return customCommandFilter(command); });
  QObject::connect(term, SIGNAL(lineReceived(QString)), &autoMap, SLOT(processLine(QString)));
  QObject::connect(term, SIGNAL(lineReceived(QString)), itemDB(), SLOT(processLine(QString)));
  QObject::connect(term, SIGNAL(lineReceived(QString)), triggers(), SLOT(processLine(QString)), Qt::QueuedConnection);
  QObject::connect(term, SIGNAL(lineReceived(QString)), this, SLOT(onLineReceived(QString)));
  QObject::connect(term, SIGNAL(commandEntered(QString, bool)), &autoMap, SLOT(commandEntered(QString, bool)), Qt::QueuedConnection);
  QObject::connect(term, SIGNAL(speedwalk(QStringList)), this, SLOT(speedwalk(QStringList)));

  QObject::connect(term->socket(), SIGNAL(connected()), this, SIGNAL(statusUpdated()));
  QObject::connect(term->socket(), SIGNAL(disconnected()), this, SIGNAL(statusUpdated()));
  QObject::connect(term->socket(), SIGNAL(connected()), this, SLOT(connectionChanged()));
  QObject::connect(term->socket(), SIGNAL(disconnected()), this, SLOT(connectionChanged()));
  QObject::connect(term->socket(), SIGNAL(promptWaiting()), &autoMap, SLOT(promptWaiting()));
  QObject::connect(term->socket(), SIGNAL(msspEvent(QString, QString)), this, SIGNAL(msspReceived()));
  QObject::connect(term->socket(), SIGNAL(gmcpEvent(QString, QVariant)), &autoMap, SLOT(gmcpEvent(QString, QVariant)));
  QObject::connect(term->socket(), SIGNAL(gmcpEvent(QString, QVariant)), this, SLOT(gmcpEvent(QString, QVariant)));
  QObject::connect(term->socket(), SIGNAL(serverCertificate(QMap<QString,QString>,bool,bool)), this, SLOT(serverCertificate(QMap<QString,QString>,bool,bool)));

  infoModel = new InfoModel(this);

  addCommand(new IdentifyCommand(&profile->serverProfile->itemDB));
  addCommand(new EquipmentCommand(this));
  addCommand(new SpeedwalkCommand(map(), &exploreHistory, [this](const QString& step, CommandResult& res, bool fast){ speedwalkStep(step, res, fast); }, false));
  addCommand(new SlotCommand(".", this, SLOT(abortSpeedwalk()), "Aborts a speedwalk path in progress"));
  addCommand(new SlotCommand("DC", term->socket(), SLOT(disconnectFromHost()), "Disconnects from the game"))->addKeyword("DISCONNECT");
  addCommand(new SlotCommand("EXPLORE", this, SLOT(exploreMap()), "Opens the map exploration window"))->addKeyword("MAP");

  RouteCommand* routeCommand = addCommand(new RouteCommand(map(), &exploreHistory));
  QObject::connect(routeCommand, SIGNAL(speedwalk(QStringList)), this, SLOT(speedwalk(QStringList)));

  WaypointCommand* waypointCommand = addCommand(new WaypointCommand(map(), &exploreHistory));
  QObject::connect(waypointCommand, SIGNAL(speedwalk(QStringList)), this, SLOT(speedwalk(QStringList)));

  QObject::connect(term, SIGNAL(slashCommand(QString, QStringList)), this, SLOT(processSlashCommand(QString, QStringList)));
  QObject::connect(triggers(), SIGNAL(executeCommand(QString, bool)), term, SLOT(processCommand(QString, bool)), Qt::QueuedConnection);

  QObject::connect(&autoMap, SIGNAL(currentRoomUpdated(int)), this, SLOT(setLastRoom(int)));

  term->installEventFilter(this);
  equipResult = CommandResult::success();
  stepResult = CommandResult::success();
  customResult = CommandResult::success();
  stepTimer.setInterval(3000);
  stepTimer.setSingleShot(true);
  QObject::connect(&stepTimer, SIGNAL(timeout()), this, SLOT(stepTimeout()));
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
    term->socket()->connectToHost(profile->host, profile->port, profile->tls);
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
  reload();
}

void GaloshSession::reload()
{
  profile->reload();
  QSettings globalSettings;

  QString colors = profile->colorScheme.isEmpty() ? ColorSchemes::defaultScheme() : profile->colorScheme;
  term->setColorScheme(ColorSchemes::scheme(colors));
  term->setTermFont(profile->font());
}

void GaloshSession::serverCertificate(const QMap<QString, QString>& info, bool selfSigned, bool nameMismatch)
{
  QString host = term->socket()->hostname();
  term->writeColorLine("1;4;95", QStringLiteral("Security certificate for %1:").arg(host).toUtf8());
  for (auto [key, value] : cpairs(info)) {
    QString message = QStringLiteral("\t%1\t%2").arg(key).arg(value);
    if (key == "issuer" && selfSigned) {
      message = "\tissuer\t\e[31m(self-signed)";
    } else if (key == "CN" && nameMismatch) {
      message = "\tCN\t\e[31m" + value;
    }
    term->writeColorLine("35", message.toUtf8());
  }

  bool newHash = profile->serverProfile->certificateHash.isEmpty();
  bool changedHash = profile->serverProfile->certificateHash != info["sha256"];
  if (nameMismatch && (newHash || changedHash)) {
    QString error = QStringLiteral("The certificate does not appear to belong to %1!").arg(host);
    term->writeColorLine("1;91", QStringLiteral("* \e[5m%1:\e[25m %2 *").arg("WARNING").arg(error).toUtf8());
    term->writeColorLine("91", "  Proceed with caution.");
    term->writeColorLine("93", "  This warning will not appear again unless the certificate changes.");
  }
  if (newHash) {
    profile->serverProfile->certificateHash = info["sha256"];
    profile->serverProfile->save();
    if (selfSigned) {
      QString warning = "This certificate is self-signed.";
      term->writeColorLine("1;93", QStringLiteral("* \e[5m%1:\e[25m %2 *").arg("NOTICE").arg(warning).toUtf8());
      term->writeColorLine("93", "  The identity of this server cannot be verified.");
      term->writeColorLine("93", "  This warning will not appear again unless the certificate changes.");
    }
  } else if (changedHash) {
    QString warning = "The certificate has changed since the last connection.";
    term->writeColorLine("1;93", QStringLiteral("* \e[5m%1:\e[25m %2 *").arg("NOTICE").arg(warning).toUtf8());
    term->writeColorLine("93", "  This may be normal, but verify that the certificate is correct.");
  }
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
  if (!stepResult.isFinished()) {
    stepResult.done(true);
    speedPath.clear();
    stepTimer.stop();
    return;
  }
  if (commandQueue.isEmpty() && speedPath.isEmpty()) {
    term->showError("No speedwalk to abort.");
  } else {
    bool wasSpeedwalk = !speedPath.isEmpty();
    commandQueue.clear();
    speedPath.clear();
    if (wasSpeedwalk) {
      term->writeColorLine("96", "Speedwalk aborted.");
    } else {
      term->writeColorLine("96", "Command aborted.");
    }
  }
}

void GaloshSession::setLastRoom(int roomId)
{
  profile->setLastRoomId(roomId);
  exploreHistory.goTo(roomId);
  emit currentRoomUpdated();
  stepTimer.stop();
  if (!stepResult.isFinished()) {
    stepResult.done(false);
  }
  /*
  if (!speedPath.isEmpty()) {
    QString dir = speedPath.takeFirst();
    if (dir == "done") {
      term->writeColorLine("93", "Speedwalk complete.");
      processCommandQueue();
    } else {
      term->processCommand(dir);
      if (speedPath.isEmpty()) {
        speedPath << "done";
      }
    }
  }
  */
}

void GaloshSession::showCommandMessage(TextCommand* command, const QString& message, TextCommandProcessor::MessageType msgType)
{
  QString formatted = QString(message).replace("\n", "\r\n").replace("\r\r", "\r");
  if (msgType == MT_Error) {
    if (command) {
      term->showError(command->name() + ": " + formatted);
    } else {
      term->showError(formatted);
    }
  } else if (msgType == MT_Subcommand) {
    term->writeColorLine("93", formatted.toUtf8());
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

void GaloshSession::onLineReceived(const QString& line)
{
  setUnread();
  if (!stepResult.isFinished()) {
    // TODO: make triggers configurable
    if (line.contains("Alas, you cannot go that way") || line.endsWith("seems to be closed.")) {
      abortSpeedwalk();
    }
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

CommandResult GaloshSession::switchEquipment(const QString& set, const QString& container)
{
  if (!equipResult.isFinished()) {
    term->showError("Another equipment command is in progress.");
    return CommandResult::fail();
  }
  equipResult = CommandResult();
  QString setName = set;
  itemDB()->captureEquipment(term, [this, set, container](const ItemDatabase::EquipmentSet& eq){ changeEquipment(eq, set, container); });
  // TODO: customize eq command (but this works for every MUD I've ever seen)
  term->processCommand("equipment");
  return equipResult;
}

void GaloshSession::equipmentReceived(const ItemDatabase::EquipmentSet& equipment)
{
  ItemSetDialog dlg(profile.get(), true, term);
  dlg.loadNewEquipment(equipment);
  dlg.exec();
}

static ItemDatabase::EquipmentSet addSlotNumbers(ItemDatabase::EquipmentSet set)
{
  QMap<QString, int> slotCount;
  for (ItemDatabase::EquipSlot& slot : set)
  {
    int count = (slotCount[slot.location] += 1);
    if (count > 1) {
      slot.location += QStringLiteral(".%1").arg(count);
    }
  }
  return set;
}

static ItemDatabase::EquipmentSet diffSets(const ItemDatabase* db, const ItemDatabase::EquipmentSet& lhs, const ItemDatabase::EquipmentSet& rhs, bool ignoreBlank)
{
  ItemDatabase::EquipmentSet result;

  QSet<QString> claimed;
  for (const ItemDatabase::EquipSlot& lSlot : lhs) {
    if (lSlot.displayName.isEmpty() || lSlot.location == "_container") {
      continue;
    }
    QString slotName = lSlot.location.section('.', 0, 0);
    bool found = false, ignore = true;
    for (const ItemDatabase::EquipSlot& rSlot : rhs) {
      QString rSlotName = rSlot.location.section('.', 0, 0);
      if (slotName != rSlotName || rSlotName == "_container") {
        continue;
      }
      ignore = false;
      if (lSlot.displayName == rSlot.displayName) {
        if (claimed.contains(rSlot.location)) {
          continue;
        }
        claimed << rSlot.location;
        found = true;
        break;
      }
    }
    if (!found && (!ignoreBlank || !ignore)) {
      result << (ItemDatabase::EquipSlot){ db->equipmentSlotType(slotName).keyword, lSlot.displayName };
    }
  }

  return result;
}

void GaloshSession::changeEquipment(const ItemDatabase::EquipmentSet& _current, const QString& setName, const QString& toContainer)
{
  ItemDatabase* db = itemDB();
  ItemDatabase::EquipmentSet current = addSlotNumbers(_current);
  ItemDatabase::EquipmentSet target = addSlotNumbers(profile->loadItemSet(setName));
  auto toRemove = diffSets(db, current, target, true);
  auto toEquip = diffSets(db, target, current, false);

  QString fromContainer;
  for (auto [slot, name] : target) {
    if (slot == "_container") {
      fromContainer = db->itemKeyword(name);
      if (fromContainer.isEmpty()) {
        fromContainer = name.section(' ', -1);
        term->writeColorLine("1;96", QStringLiteral("Keyword not set for \"%1\", using \"%2\"").arg(name).arg(fromContainer).toUtf8());
      }
      break;
    }
  }

  term->writeColorLine("", "");
  for (auto [slot, item] : toRemove) {
    QString itemKeyword = db->itemKeyword(item);
    if (itemKeyword.isEmpty()) {
      itemKeyword = item.section(' ', -1);
      term->writeColorLine("1;96", QStringLiteral("Keyword not set for \"%1\", using \"%2\"").arg(item).arg(itemKeyword).toUtf8());
    }
    term->processCommand(QStringLiteral("remove %1").arg(itemKeyword));
    if (!toContainer.isEmpty()) {
      term->processCommand(QStringLiteral("put %1 %2").arg(itemKeyword).arg(toContainer));
    }
  }
  for (auto [slot, item] : toEquip) {
    QString itemKeyword = db->itemKeyword(item);
    if (itemKeyword.isEmpty()) {
      itemKeyword = item.section(' ', -1);
      term->writeColorLine("1;96", QStringLiteral("Keyword not set for \"%1\", using \"%2\"").arg(item).arg(itemKeyword).toUtf8());
    }
    if (!fromContainer.isEmpty()) {
      term->processCommand(QStringLiteral("get %1 %2").arg(itemKeyword).arg(fromContainer));
    }
    QString verb = db->parsers.verbs.value(slot);
    if (verb.isEmpty()) {
      term->processCommand(QStringLiteral("wear %1 %2").arg(itemKeyword).arg(slot.toLower()));
    } else {
      term->processCommand(verb.arg(itemKeyword));
    }
  }

  equipResult.done(false);
}

void GaloshSession::openItemDatabase()
{
  if (!itemSearch) {
    itemSearch = new ItemSearchDialog(itemDB(), false, term);
    itemSearch->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(itemSearch, SIGNAL(openItemSets()), this, SLOT(openItemSets()));
  }
  itemSearch->show();
  itemSearch->raise();
  itemSearch->setFocus();
}

void GaloshSession::openItemSets()
{
  if (itemSearch) {
    itemSearch->close();
  }
  if (!itemSets) {
    itemSets = new ItemSetDialog(profile.get(), false, term);
    itemSets->setAttribute(Qt::WA_DeleteOnClose);
    itemSets->setConnected(term->socket()->isConnected());
    QObject::connect(itemSets, SIGNAL(equipNamedSet(QString)), this, SLOT(switchEquipment(QString)));
  }
  itemSets->show();
  itemSets->raise();
  itemSets->setFocus();
}

void GaloshSession::connectionChanged()
{
  bool conn = term->socket()->isConnected();
  if (itemSets) {
    itemSets->setConnected(conn);
  }
}

void GaloshSession::processCommandQueue(const CommandResult* result)
{
  if (result && result->hasError()) {
    term->showError("Command aborted due to error.");
    commandQueue.clear();
    return;
  }
  if (!customResult.isFinished()) {
    // this was triggered while a custom command was already running
    return;
  }
  if (commandQueue.isEmpty()) {
    return;
  }
  QString command = commandQueue.takeFirst();
  term->processCommand(command);
  if (customResult.isFinished()) {
    QTimer::singleShot(0, this, SLOT(processCommandQueue()));
  }
}

void GaloshSession::stepTimeout()
{
  term->showError("Speedwalking appears to have stalled. Aborting.");
  speedPath.clear();
  if (!stepResult.isFinished()) {
    stepResult.done(true);
  }
}

void GaloshSession::speedwalkStep(const QString& step, CommandResult& res, bool fast)
{
  if (!fast) {
    stepResult = res;
    stepTimer.start();
  }
  term->processCommand(step);
}

// like with eventFilter, return true to stop commands from being further handled
bool GaloshSession::customCommandFilter(const QString& command)
{
  auto [cmd, args] = parseCommand(command);
  QStringList actions = profile->customCommand(cmd);
  if (actions.isEmpty()) {
    return false;
  }

  static QRegularExpression argRE("(?<!%)%(\\d+|[*]|{\\d+[:][^}]+})([+]?)");
  QStringList parsed;
  for (QString action : actions) {
    int pos = 0;
    while (pos < action.length()) {
      auto match = argRE.match(action, pos);
      if (!match.hasMatch()) {
        break;
      }
      QString ref = match.captured(1);
      QString replacement = ref.section(':', 1);
      replacement.chop(1);
      int index = ref.section(':', 0, 0).toInt() - 1;
      if (index < 0) {
        index = 0;
      }
      bool rest = !match.captured(2).isEmpty();
      if (index < args.length()) {
        if (rest) {
          replacement = args.mid(index).join(' ');
        } else {
          replacement = args[index];
        }
      }
      action = action.left(match.capturedStart(0)) + replacement + action.mid(match.capturedEnd(0));
      pos = match.capturedStart(0) + replacement.length();
    }
    parsed << action;
  }
  commandQueue = parsed + commandQueue;
  processCommandQueue();
  return true;
}

void GaloshSession::processSlashCommand(const QString& command, const QStringList& args)
{
  if (!customResult.isFinished() || !equipResult.isFinished() || !stepResult.isFinished()) {
    if (command != ".") {
      term->showError("Another command is in progress.");
      return;
    }
  }
  customResult = handleCommand(command, args);
  if (customResult.isFinished()) {
    if (customResult.hasError()) {
      processCommandQueue(&customResult);
    } else {
      QTimer::singleShot(0, this, SLOT(processCommandQueue()));
    }
  } else {
    customResult.setCallback(this, &GaloshSession::processCommandQueue);
  }
}
