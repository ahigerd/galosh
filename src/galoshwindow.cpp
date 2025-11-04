#include "galoshwindow.h"
#include "galoshterm.h"
#include "telnetsocket.h"
#include "infomodel.h"
#include "roomview.h"
#include "msspview.h"
#include "exploredialog.h"
#include <QDesktopServices>
#include <QApplication>
#include <QMessageBox>
#include <QSettings>
#include <QDockWidget>
#include <QTreeView>
#include <QHeaderView>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QToolButton>
#include <QLabel>
#include <QEvent>
#include <QtDebug>

GaloshWindow::GaloshWindow(QWidget* parent)
: QMainWindow(parent), explore(nullptr), lastRoomId(-1), geometryReady(false)
{
  setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
  setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

  term = new GaloshTerm(this);
  setCentralWidget(term);

  infoModel = new InfoModel(this);
  QObject::connect(term->socket(), SIGNAL(msspEvent(QString, QString)), this, SLOT(msspEvent(QString, QString)));
  QObject::connect(term->socket(), SIGNAL(gmcpEvent(QString, QVariant)), this, SLOT(gmcpEvent(QString, QVariant)));

  infoDock = new QDockWidget(this);
  infoDock->setWindowTitle("Character Stats");
  infoDock->setObjectName("infoView");
  infoView = new QTreeView(infoDock);
  infoView->setModel(infoModel);
  infoView->setItemsExpandable(false);
  infoView->header()->setStretchLastSection(true);
  infoDock->setWidget(infoView);
  addDockWidget(Qt::RightDockWidgetArea, infoDock);

  roomDock = new QDockWidget(this);
  roomDock->setObjectName("roomView");
  roomView = new RoomView(this);
  roomDock->setWidget(roomView);
  QObject::connect(roomView, SIGNAL(roomUpdated(QString, int)), this, SLOT(setLastRoom(QString, int)));
  QObject::connect(roomView, SIGNAL(exploreRoom(int, QString)), this, SLOT(exploreMap(int, QString)));
  addDockWidget(Qt::TopDockWidgetArea, roomDock);

  QMenuBar* mb = new QMenuBar(this);
  setMenuBar(mb);

  QMenu* fileMenu = new QMenu("&File", mb);
  fileMenu->addAction("&Connect...", this, SLOT(openConnectDialog()));
  fileMenu->addAction("&Disconnect", term->socket(), SLOT(disconnectFromHost()));
  fileMenu->addSeparator();
  fileMenu->addAction("E&xit", qApp, SLOT(quit()));
  mb->addMenu(fileMenu);

  QMenu* viewMenu = new QMenu("&View", mb);
  viewMenu->addAction("&Profiles...", this, SLOT(openProfileDialog()));
  viewMenu->addSeparator();
  msspMenu = viewMenu->addAction("View &MSSP Info...", this, SLOT(openMsspDialog()));
  exploreAction = viewMenu->addAction("E&xplore Map...", this, SLOT(exploreMap()));
  exploreAction->setEnabled(false);
  viewMenu->addSeparator();
  roomAction = viewMenu->addAction("&Room Description", this, SLOT(toggleRoomDock(bool)));
  roomAction->setCheckable(true);
  infoAction = viewMenu->addAction("Character &Stats", this, SLOT(toggleInfoDock(bool)));
  infoAction->setCheckable(true);
  viewMenu->addSeparator();
  viewMenu->addAction("Open &Configuration Folder...", this, SLOT(openConfigFolder()));
  mb->addMenu(viewMenu);

  QMenu* helpMenu = new QMenu("&Help", mb);
  helpMenu->addAction("Open &Website...", this, SLOT(openWebsite()));
  helpMenu->addSeparator();
  helpMenu->addAction("&About...", this, SLOT(about()));
  helpMenu->addAction("About &Qt...", qApp, SLOT(aboutQt()));
  mb->addMenu(helpMenu);

  QToolBar* tb = new QToolBar(this);
  tb->setObjectName("toolbar");
  tb->addAction("Connect", this, SLOT(openConnectDialog()));
  tb->addAction("Disconnect", term->socket(), SLOT(disconnectFromHost()));
  tb->addSeparator();
  tb->addAction("Triggers", [this]{ openProfileDialog(ProfileDialog::TriggersTab); });
  tb->addSeparator();
  msspButton = tb->addAction("MSSP", this, SLOT(openMsspDialog()));
  addToolBar(tb);

  QStatusBar* bar = new QStatusBar(this);
  bar->setSizeGripEnabled(false);
  setStatusBar(bar);

  sbStatus = new QLabel("Disconnected", bar);
  sbStatus->setFrameStyle(QFrame::Sunken | QFrame::Panel);
  bar->addWidget(sbStatus, 1);

  resize(800, 600);
  fixGeometry = true;

  QObject::connect(term, SIGNAL(lineReceived(QString)), &map, SLOT(processLine(QString)));
  QObject::connect(term, SIGNAL(lineReceived(QString)), &triggers, SLOT(processLine(QString)), Qt::QueuedConnection);
  QObject::connect(term, SIGNAL(commandEntered(QString, bool)), &map, SLOT(commandEntered(QString, bool)), Qt::QueuedConnection);

  QObject::connect(term->socket(), SIGNAL(connected()), this, SLOT(updateStatus()));
  QObject::connect(term->socket(), SIGNAL(disconnected()), this, SLOT(updateStatus()));
  QObject::connect(term->socket(), SIGNAL(promptWaiting()), &map, SLOT(promptWaiting()));
  QObject::connect(term->socket(), SIGNAL(gmcpEvent(QString, QVariant)), &map, SLOT(gmcpEvent(QString, QVariant)));

  QObject::connect(&triggers, SIGNAL(executeCommand(QString, bool)), term, SLOT(executeCommand(QString, bool)), Qt::QueuedConnection);

  QObject::connect(&map, SIGNAL(currentRoomUpdated(MapManager*, int)), roomView, SLOT(setRoom(MapManager*, int)));

  for (QAction* action : tb->actions()) {
    QToolButton* b = qobject_cast<QToolButton*>(tb->widgetForAction(action));
    if (b) {
      b->setAutoRaise(false);
    }
  }

  infoDock->installEventFilter(this);
  roomDock->installEventFilter(this);
  stateThrottle.setSingleShot(true);
  stateThrottle.setInterval(100);
  QObject::connect(&stateThrottle, SIGNAL(timeout()), this, SLOT(updateGeometry()));
  QObject::connect(infoView->header(), &QHeaderView::sectionResized, [this](int, int, int){ updateGeometry(true); });

  updateStatus();
}

void GaloshWindow::showEvent(QShowEvent* event)
{
  QMainWindow::showEvent(event);
  fixGeometry = true;
}

void GaloshWindow::paintEvent(QPaintEvent* event)
{
  if (fixGeometry) {
    QSettings settings;
    restoreGeometry(settings.value("window").toByteArray());
    restoreState(settings.value("docks").toByteArray());
    QStringList sizes = settings.value("infoColumns").toStringList();
    for (int i = 0; i < sizes.size(); i++) {
      infoView->setColumnWidth(i, sizes[i].toInt());
    }
    infoAction->setChecked(infoView->isVisible());
    roomAction->setChecked(roomView->isVisible());

    if (pos().x() < 0 || pos().y() < 0) {
      move(0, 0);
    }
    fixGeometry = false;
    geometryReady = true;
  }
  QMainWindow::paintEvent(event);
}

void GaloshWindow::updateStatus()
{
  if (term->socket()->isConnected()) {
    QString host = term->socket()->hostname();
    sbStatus->setText(QStringLiteral("Connected to %1.").arg(host));
  } else {
    sbStatus->setText("Disconnected.");
  }
  msspMenu->setEnabled(msspAvailable());
  msspButton->setEnabled(msspAvailable());
}

void GaloshWindow::msspEvent(const QString&, const QString&)
{
  msspMenu->setEnabled(true);
  msspButton->setEnabled(true);
}

void GaloshWindow::gmcpEvent(const QString& key, const QVariant& value)
{
  if (key.toUpper() == "CHAR") {
    infoModel->loadTree(value);
    infoView->expandAll();
  } else if (key.toUpper() == "EXTERNAL.DISCORD.STATUS") {
    QVariantMap map = value.toMap();
    QString state = map["state"].toString();
    QString details = map["details"].toString();
    QStringList parts;
    if (!state.isEmpty()) {
      parts << state;
    }
    if( !details.isEmpty()) {
      parts << details;
    }
    if (!parts.isEmpty()) {
      sbStatus->setText(parts.join(" - "));
    }
  } else if (key.toUpper() != "ROOM") {
    qDebug() << key << value;
  }
}

void GaloshWindow::openConnectDialog()
{
  ProfileDialog* dlg = new ProfileDialog(true, this);
  QObject::connect(dlg, SIGNAL(connectToProfile(QString, bool)), this, SLOT(connectToProfile(QString, bool)));
  dlg->open();
}

void GaloshWindow::openProfileDialog(ProfileDialog::Tab tab)
{
  ProfileDialog* dlg = new ProfileDialog(tab, this);
  QObject::connect(dlg, SIGNAL(profileUpdated(QString)), this, SLOT(reloadProfile(QString)));
  dlg->show();
}

void GaloshWindow::connectToProfile(const QString& path, bool online)
{
  currentProfile = path;
  reloadProfile(path);
  QSettings settings(path, QSettings::IniFormat);
  settings.beginGroup("Profile");
  map.loadProfile(path);
  if (online) {
    QString command = settings.value("commandLine").toString();
    if (command.isEmpty()) {
      term->socket()->connectToHost(settings.value("host").toString(), settings.value("port").toInt());
    } else {
      term->socket()->connectCommand(command);
    }
  } else {
    term->socket()->setHost(settings.value("host").toString(), settings.value("port").toInt());
    roomView->setRoom(&map, settings.value("lastRoom", -1).toInt());
  }
}

void GaloshWindow::reloadProfile(const QString& path)
{
  if (path != currentProfile) {
    return;
  }
  triggers.loadProfile(path);
  QSettings settings(path, QSettings::IniFormat);
  settings.beginGroup("Profile");
  setWindowTitle(settings.value("name").toString());
}

void GaloshWindow::moveEvent(QMoveEvent*)
{
  updateGeometry(true);
}

void GaloshWindow::resizeEvent(QResizeEvent*)
{
  updateGeometry(true);
}

bool GaloshWindow::eventFilter(QObject*, QEvent* event)
{
  if (event->type() == QEvent::Resize || event->type() == QEvent::Move || event->type() == QEvent::Close) {
    updateGeometry(true);
  }
  return false;
}

void GaloshWindow::updateGeometry(bool queue)
{
  if (queue) {
    stateThrottle.start();
    return;
  }
  if (isVisible()) {
    QSettings settings;
    settings.setValue("window", saveGeometry());
    settings.setValue("docks", saveState());

    QStringList sizes;
    for (int i = 0; i < infoModel->columnCount(); i++) {
      sizes << QString::number(infoView->columnWidth(i));
    }
    settings.setValue("infoColumns", sizes);

    infoAction->setChecked(infoView->isVisible());
    roomAction->setChecked(roomView->isVisible());
  }
}

void GaloshWindow::toggleRoomDock(bool checked)
{
  roomDock->setVisible(checked);
  updateGeometry(false);
}

void GaloshWindow::toggleInfoDock(bool checked)
{
  infoDock->setVisible(checked);
  updateGeometry(false);
}

void GaloshWindow::openConfigFolder()
{
  QDesktopServices::openUrl(QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)));
}

void GaloshWindow::openWebsite()
{
  QDesktopServices::openUrl(QUrl("https://github.com/ahigerd/galosh"));
}

void GaloshWindow::about()
{
  QFile html(":/about.html");
  html.open(QIODevice::ReadOnly);
  QMessageBox::about(this, "About Galosh", QString::fromUtf8(html.readAll()));
}

bool GaloshWindow::msspAvailable() const
{
  if (!term->socket()) {
    return false;
  }

  return !term->socket()->hostname().isEmpty() && !term->socket()->mssp.isEmpty();
}

void GaloshWindow::openMsspDialog()
{
  if (!msspAvailable()) {
    return;
  }

  (new MsspView(term->socket(), this))->open();
}

void GaloshWindow::setLastRoom(const QString& title, int roomId)
{
  roomDock->setWindowTitle(title);
  QSettings settings(currentProfile, QSettings::IniFormat);
  settings.beginGroup("Profile");
  settings.value("lastRoom", roomId);
  lastRoomId = roomId;
  exploreAction->setEnabled(true);
}

void GaloshWindow::exploreMap(int roomId, const QString& movement)
{
  if (explore) {
    explore->roomView()->setRoom(&map, roomId, movement);
    explore->refocus();
  } else {
    explore = new ExploreDialog(&map, roomId, lastRoomId, movement, this);
    QObject::connect(explore, SIGNAL(exploreRoom(int, QString)), this, SLOT(exploreMap(int, QString)));
    explore->show();
  }
}
