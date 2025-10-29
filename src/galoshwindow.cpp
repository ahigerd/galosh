#include "galoshwindow.h"
#include "galoshterm.h"
#include "telnetsocket.h"
#include "infomodel.h"
#include "roomview.h"
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
: QMainWindow(parent), geometryReady(false)
{
  setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
  setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

  term = new GaloshTerm(this);
  setCentralWidget(term);

  infoModel = new InfoModel(this);
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
  QObject::connect(roomView, SIGNAL(roomUpdated(QString)), roomDock, SLOT(setWindowTitle(QString)));
  addDockWidget(Qt::TopDockWidgetArea, roomDock);

  QMenuBar* mb = new QMenuBar(this);
  setMenuBar(mb);

  QMenu* fileMenu = new QMenu("&File", mb);
  fileMenu->addAction("&Connect...", this, SLOT(openConnectDialog()));
  fileMenu->addSeparator();
  fileMenu->addAction("E&xit", qApp, SLOT(quit()));
  mb->addMenu(fileMenu);

  QMenu* viewMenu = new QMenu("&View", mb);
  viewMenu->addAction("&Profiles...", this, SLOT(openProfileDialog()));
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
  tb->addAction("Triggers", [this]{ openProfileDialog(ProfileDialog::TriggersTab); });
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

  QObject::connect(term->socket(), SIGNAL(connected()), this, SLOT(updateStatus()));
  QObject::connect(term->socket(), SIGNAL(disconnected()), this, SLOT(updateStatus()));
  QObject::connect(term->socket(), SIGNAL(msspEvent(QString, QString)), &map, SLOT(msspEvent(QString, QString)));
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
    setWindowTitle(QStringLiteral("Galosh - %1").arg(host));
  } else {
    sbStatus->setText("Disconnected.");
    setWindowTitle("Galosh");
  }
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
  QObject::connect(dlg, SIGNAL(connectToProfile(QString)), this, SLOT(connectToProfile(QString)));
  dlg->open();
}

void GaloshWindow::openProfileDialog(ProfileDialog::Tab tab)
{
  ProfileDialog* dlg = new ProfileDialog(tab, this);
  QObject::connect(dlg, SIGNAL(profileUpdated(QString)), this, SLOT(reloadProfile(QString)));
  dlg->show();
}

void GaloshWindow::connectToProfile(const QString& path)
{
  currentProfile = path;
  triggers.loadProfile(path);
  QSettings settings(path, QSettings::IniFormat);
  settings.beginGroup("Profile");
  term->socket()->connectToHost(settings.value("host").toString(), settings.value("port").toInt());
  map.loadProfile(path);
}

void GaloshWindow::reloadProfile(const QString& path)
{
  if (path != currentProfile) {
    return;
  }
  triggers.loadProfile(path);
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
