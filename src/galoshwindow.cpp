#include "galoshwindow.h"
#include "galoshsession.h"
#include "galoshterm.h"
#include "userprofile.h"
#include "telnetsocket.h"
#include "infomodel.h"
#include "roomview.h"
#include "msspview.h"
#include "exploredialog.h"
#include "mapviewer.h"
#include "mapoptions.h"
#include <QDesktopServices>
#include <QApplication>
#include <QFontDatabase>
#include <QMessageBox>
#include <QSettings>
#include <QDockWidget>
#include <QTabWidget>
#include <QStackedWidget>
#include <QTreeView>
#include <QHeaderView>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QToolButton>
#include <QLabel>
#include <QEvent>
#include <QtDebug>

enum class TabStatus {
  Disconnected,
  Idle,
  Updated,
};

static QMap<TabStatus, QIcon> loadTabIcons()
{
  QMap<TabStatus, QIcon> icons;
  icons[TabStatus::Disconnected].addFile(":/red.png");
  icons[TabStatus::Idle].addFile(":/gray.png");
  icons[TabStatus::Updated].addFile(":/green.png");
  return icons;
}

static QMap<TabStatus, QIcon> tabIcons;

GaloshWindow::GaloshWindow(QWidget* parent)
: QMainWindow(parent), mapDockView(nullptr), geometryReady(false), shouldRestoreDocks(false)
{
  tabIcons = loadTabIcons();

  setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
  setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

  stackedWidget = new QStackedWidget(this);
  setCentralWidget(stackedWidget);

  tabs = new QTabWidget(stackedWidget);
  tabs->setIconSize(QSize(12, 12));
  tabs->setTabsClosable(true);
  tabs->setMovable(true);
  tabs->setDocumentMode(true);
  QObject::connect(tabs, SIGNAL(currentChanged(int)), this, SLOT(updateStatus()));
  QObject::connect(tabs, SIGNAL(tabCloseRequested(int)), this, SLOT(closeSession(int)));
  stackedWidget->addWidget(tabs);

  background = new QWidget(stackedWidget);
  {
    QPalette p = palette();
    p.setBrush(QPalette::Window, ColorSchemes::scheme(ColorSchemes::defaultScheme())[ColorScheme::Background]);
    background->setBackgroundRole(QPalette::Window);
    background->setAutoFillBackground(true);
    background->setPalette(p);
  }
  stackedWidget->addWidget(background);
  stackedWidget->setCurrentWidget(background);

  infoDock = new QDockWidget(this);
  infoDock->setWindowTitle("Character Stats");
  infoDock->setObjectName("infoView");
  infoView = new QTreeView(infoDock);
  infoView->setItemsExpandable(false);
  infoView->header()->setStretchLastSection(true);
  infoDock->setWidget(infoView);
  addDockWidget(Qt::RightDockWidgetArea, infoDock);

  roomDock = new QDockWidget(this);
  roomDock->setObjectName("roomView");
  roomView = new RoomView(this);
  roomDock->setWidget(roomView);
  QObject::connect(roomView, SIGNAL(roomUpdated(QString,int,QString)), roomDock, SLOT(setWindowTitle(QString)));
  QObject::connect(roomView, SIGNAL(exploreRoom(int, QString)), this, SLOT(exploreMap(int, QString)));
  addDockWidget(Qt::TopDockWidgetArea, roomDock);

  mapDock = new QDockWidget(this);
  mapDock->setObjectName("mapView");
  mapDock->setWindowTitle("Mini-Map");
  mapDockView = new MapViewer(MapViewer::MiniMap, this);
  mapDock->setWidget(mapDockView);
  addDockWidget(Qt::TopDockWidgetArea, mapDock);

  QMenuBar* mb = new QMenuBar(this);
  setMenuBar(mb);

  QMenu* fileMenu = new QMenu("&File", mb);
  fileMenu->addAction("C&onnect...", this, SLOT(openConnectDialog()));
  disconnectedActions << fileMenu->addAction("&Reconnect", this, SLOT(reconnectSession()));
  connectedActions << fileMenu->addAction("&Disconnect", this, SLOT(disconnectSession()));
  profileActions << fileMenu->addAction("&Close", this, SLOT(closeSession()), QKeySequence::Close);
  fileMenu->addSeparator();
  fileMenu->addAction("E&xit", qApp, SLOT(quit()));
  mb->addMenu(fileMenu);

  QMenu* viewMenu = new QMenu("&View", mb);
  viewMenu->addAction("&Profiles...", this, SLOT(openProfileDialog()));
  profileActions << viewMenu->addAction("&Map Settings...", this, SLOT(openMapOptions()));
  viewMenu->addSeparator();
  msspMenu = viewMenu->addAction("View MSSP &Info...", this, SLOT(openMsspDialog()));
  profileActions << viewMenu->addAction("E&xplore Map...", this, SLOT(exploreMap()));
  viewMenu->addSeparator();
  roomAction = viewMenu->addAction("&Room Description", this, SLOT(toggleRoomDock(bool)));
  roomAction->setCheckable(true);
  infoAction = viewMenu->addAction("Character &Stats", this, SLOT(toggleInfoDock(bool)));
  infoAction->setCheckable(true);
  mapDockAction = viewMenu->addAction("Mini-Ma&p", this, SLOT(toggleMapDock(bool)));
  mapDockAction->setCheckable(true);
  viewMenu->addSeparator();
  viewMenu->addAction("Open &Configuration Folder...", this, SLOT(openConfigFolder()));
  mb->addMenu(viewMenu);

  QMenu* helpMenu = new QMenu("&Help", mb);
  helpMenu->addAction("Open &Website...", this, SLOT(openWebsite()));
  profileActions << helpMenu->addAction("Show Command &Help", this, SLOT(help()));
  helpMenu->addSeparator();
  helpMenu->addAction("&About...", this, SLOT(about()));
  helpMenu->addAction("About &Qt...", qApp, SLOT(aboutQt()));
  mb->addMenu(helpMenu);

  QToolBar* tb = new QToolBar(this);
  tb->setObjectName("toolbar");
  tb->addAction("Connect", this, SLOT(openConnectDialog()));
  disconnectedActions << tb->addAction("Reconnect", this, SLOT(reconnectSession()));
  connectedActions << tb->addAction("Disconnect", this, SLOT(disconnectSession()));
  tb->addAction("Profiles", this, SLOT(openProfileDialog()));
  tb->addSeparator();
  tb->addAction("Triggers", [this]{ openProfileDialog(ProfileDialog::TriggersTab); });
  profileActions << tb->addAction("Map", this, SLOT(exploreMap()));
  tb->addSeparator();
  msspButton = tb->addAction("MSSP", this, SLOT(openMsspDialog()));
  addToolBar(tb);

  QStatusBar* bar = new QStatusBar(this);
  bar->setSizeGripEnabled(false);
  setStatusBar(bar);

  sbStatus = new QLabel("Disconnected", bar);
  sbStatus->setFrameStyle(QFrame::Sunken | QFrame::Panel);
  bar->addWidget(sbStatus, 1);

  updateActions();

  resize(800, 600);
  fixGeometry = true;

  for (QAction* action : tb->actions()) {
    QToolButton* b = qobject_cast<QToolButton*>(tb->widgetForAction(action));
    if (b) {
      b->setAutoRaise(false);
    }
  }

  infoDock->installEventFilter(this);
  roomDock->installEventFilter(this);
  mapDock->installEventFilter(this);
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

void GaloshWindow::closeEvent(QCloseEvent* event)
{
  if (!confirmClose()) {
    event->ignore();
  } else {
    QMainWindow::closeEvent(event);
  }
}

void GaloshWindow::paintEvent(QPaintEvent* event)
{
  if (fixGeometry) {
    QSettings settings;
    if (!settings.value("maximized").toBool()) {
      restoreGeometry(settings.value("window").toByteArray());
    }

    QStringList sizes = settings.value("infoColumns").toStringList();
    for (int i = 0; i < sizes.size(); i++) {
      infoView->setColumnWidth(i, sizes[i].toInt());
    }
    infoAction->setChecked(infoView->isVisible());
    roomAction->setChecked(roomView->isVisible());
    mapDockAction->setChecked(mapDockView->isVisible());

    if (pos().x() < 0 || pos().y() < 0) {
      move(0, 0);
    }
    fixGeometry = false;
    geometryReady = true;
    shouldRestoreDocks = true;
    QTimer::singleShot(16, [this]{ resizeEvent(nullptr); });
  }
  QMainWindow::paintEvent(event);
}

void GaloshWindow::updateActions()
{
  bool hasProfile = !!session();
  bool hasConnection = hasProfile && session()->isConnected();
  for (QAction* action : profileActions) {
    action->setEnabled(hasProfile);
  }
  for (QAction* action : connectedActions) {
    action->setEnabled(hasConnection);
  }
  for (QAction* action : disconnectedActions) {
    action->setEnabled(hasProfile && !hasConnection);
  }
}

GaloshSession* GaloshWindow::session() const
{
  return sessions.value(tabs->currentWidget());
}

void GaloshWindow::help()
{
  GaloshSession* sess = session();
  if (!sess) {
    return;
  }
  sess->help();
}

void GaloshWindow::updateStatus()
{
  GaloshSession* sess = session();
  bool mssp = false;
  if (sess) {
    mssp = !sess->msspData().isEmpty();
    setWindowTitle(sess->name());
    sbStatus->setText(sess->statusBarText());
    if (sess->currentRoom()) {
      roomView->setRoom(sess->map(), sess->currentRoom()->id);
    } else {
      roomView->setRoom(sess->map(), -1);
    }
    infoView->setModel(sess->infoModel);
    infoView->expandAll();
  } else {
    sbStatus->setText("Disconnected.");
    roomView->setRoom(nullptr, 0);
    infoView->setModel(nullptr);
  }

  msspMenu->setEnabled(mssp);
  msspButton->setEnabled(mssp);
  updateActions();
  mapDockView->setSession(sess);

  for (int i = 0; i < tabs->count(); i++) {
    TabStatus status = TabStatus::Idle;
    GaloshSession* s = sessions[tabs->widget(i)];
    if (!s) {
      qDebug() << "XXX: tabs contains bogus widget";
      continue;
    }
    if (s->hasUnread()) {
      status = TabStatus::Updated;
    } else if (!s->isConnected()) {
      status = TabStatus::Disconnected;
    }
    tabs->setTabIcon(i, tabIcons[status]);
  }
}

void GaloshWindow::msspReceived()
{
  msspMenu->setEnabled(true);
  msspButton->setEnabled(true);
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
  GaloshSession* sess = session();
  if (sess) {
    dlg->selectProfile(sess->profile->profilePath);
  }
  QObject::connect(dlg, SIGNAL(profileUpdated(QString)), this, SLOT(reloadProfile(QString)));
  dlg->show();
}

void GaloshWindow::openMapOptions()
{
  GaloshSession* sess = session();
  if (!sess) {
    return;
  }
  MapOptions* o = new MapOptions(sess->map(), this);
  o->open();
}

GaloshSession* GaloshWindow::findSession(const QString& profilePath) const
{
  for (GaloshSession* sess : sessions) {
    if (sess->profile->profilePath == profilePath) {
      return sess;
    }
  }
  return nullptr;
}

void GaloshWindow::connectToProfile(const QString& path, bool online)
{
  GaloshSession* sess = findSession(path);
  if (sess) {
    tabs->setCurrentWidget(sess->term);
    if (sess->isConnected()) {
      return;
    }
  } else {
    UserProfile* profile = new UserProfile(path);
    // Session takes ownership of profile
    sess = new GaloshSession(profile, this);
    sessions[sess->term] = sess;
    tabs->addTab(sess->term, sess->name());
    tabs->setCurrentWidget(sess->term);
    stackedWidget->setCurrentWidget(tabs);
    updateStatus();
    QObject::connect(sess, SIGNAL(msspReceived()), this, SLOT(msspReceived()));
    QObject::connect(sess, SIGNAL(statusUpdated()), this, SLOT(updateStatus()));
    QObject::connect(sess, SIGNAL(currentRoomUpdated()), this, SLOT(updateStatus()));
    QObject::connect(sess, SIGNAL(unreadUpdated()), this, SLOT(updateStatus()));
    QObject::connect(sess, SIGNAL(openProfileDialog(ProfileDialog::Tab)), this, SLOT(openProfileDialog(ProfileDialog::Tab)));
    QObject::connect(sess->term, SIGNAL(commandEnteredForProfile(QString,QString)), this, SLOT(sendCommandToProfile(QString,QString)));

    updateActions();
  }

  if (online) {
    sess->connect();
  } else {
    sess->startOffline();
  }
  sess->term->setFocus();
}

void GaloshWindow::reloadProfile(const QString& path)
{
  GaloshSession* sess = findSession(path);
  if (sess) {
    sess->reload();
    updateStatus();
  }
}

void GaloshWindow::sendCommandToProfile(const QString& profile, const QString& command)
{
  GaloshSession* origin = sessions.value(qobject_cast<QWidget*>(sender()));
  if (!origin) {
    origin = session();
  }
  QString prefix = profile.toLower();
  for (GaloshSession* sess : sessions) {
    if (!sess) {
      continue;
    }
    QString name = sess->profile->profileName;
    if (name.toLower().startsWith(prefix)) {
      origin->term->writeColorLine("100;93", QStringLiteral("[:%1> %2").arg(name).arg(command).toUtf8());
      sess->term->processCommand(command);
      return;
    }
  }
  if (origin) {
    origin->term->writeColorLine("100;93", QStringLiteral("[:%1> %2").arg(profile).arg(command).toUtf8());
    origin->term->showError(QStringLiteral("Could not find window for profile \"%1\".").arg(profile));
  }
}

bool GaloshWindow::confirmClose(GaloshSession* sess)
{
  QStringList names;
  if (sess) {
    if (sess->isConnected()) {
      names << sess->name();
    }
  } else {
    for (const GaloshSession* sess : sessions) {
      if (sess && sess->isConnected()) {
        names << sess->name();
      }
    }
  }
  QString message;
  if (names.length() == 1) {
    message = QStringLiteral("Closing this tab will disconnect the session \"%1\". Close it anyway?").arg(names.first());
  } else if (names.length() > 1) {
    message = QStringLiteral("Closing the window will disconnect these sessions:\n\t%1\nClose it anyway?").arg(names.join("\n\t"));
  } else {
    return true;
  }
  int button = QMessageBox::question(this, "Galosh", message, QMessageBox::Ok | QMessageBox::Cancel);
  if (button != QMessageBox::Ok) {
    return false;
  }
  return true;
}

void GaloshWindow::closeSession(int index)
{
  GaloshSession* sess;
  if (index < 0) {
    sess = session();
  } else {
    sess = sessions.value(tabs->widget(index));
  }
  if (!confirmClose(sess)) {
    return;
  }
  tabs->blockSignals(true);
  sessions.remove(sess->term);
  tabs->removeTab(tabs->indexOf(sess->term));
  delete sess;
  tabs->blockSignals(false);
  if (tabs->count() == 0) {
    stackedWidget->setCurrentWidget(background);
  }
  updateStatus();
}

void GaloshWindow::moveEvent(QMoveEvent*)
{
  updateGeometry(true);
}

void GaloshWindow::resizeEvent(QResizeEvent*)
{
  if (geometryReady && shouldRestoreDocks) {
    QSettings settings;
    restoreState(settings.value("docks").toByteArray());
    shouldRestoreDocks = false;
  } else {
    updateGeometry(true);
  }
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
    if (isMaximized()) {
      settings.setValue("maximized", true);
    } else {
      settings.remove("maximized");
    }
    settings.setValue("window", saveGeometry());
    if (geometryReady && !shouldRestoreDocks) {
      settings.setValue("docks", saveState());
    }

    QAbstractItemModel* infoModel = infoView->model();
    if (infoModel) {
      QStringList sizes;
      for (int i = 0; i < infoModel->columnCount(); i++) {
        sizes << QString::number(infoView->columnWidth(i));
      }
      settings.setValue("infoColumns", sizes);
    }

    infoAction->setChecked(infoView->isVisible());
    roomAction->setChecked(roomView->isVisible());
    mapDockAction->setChecked(mapDockView->isVisible());
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

void GaloshWindow::toggleMapDock(bool checked)
{
  mapDock->setVisible(checked);
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
  QString content = QString::fromUtf8(html.readAll());
  content = content.replace("{VERSION}", qApp->applicationVersion());
  QMessageBox::about(this, "About Galosh", content);
}

void GaloshWindow::openMsspDialog()
{
  GaloshSession* sess = session();
  if (!sess) {
    return;
  }
  auto mssp = sess->msspData();
  if (!mssp.isEmpty()) {
    (new MsspView(sess->term->socket(), this))->open();
  }
}

void GaloshWindow::exploreMap(int roomId, const QString& movement)
{
  GaloshSession* sess = session();
  if (!sess) {
    return;
  }
  sess->exploreMap(roomId, movement);
}

void GaloshWindow::reconnectSession()
{
  GaloshSession* sess = session();
  if (sess) {
    sess->connect();
  }
}

void GaloshWindow::disconnectSession()
{
  GaloshSession* sess = session();
  if (sess) {
    sess->term->socket()->disconnectFromHost();
  }
}
