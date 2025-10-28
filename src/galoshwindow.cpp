#include "galoshwindow.h"
#include "galoshterm.h"
#include "telnetsocket.h"
#include "infomodel.h"
#include "roomview.h"
#include <QSettings>
#include <QSplitter>
#include <QTreeView>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QLabel>
#include <QEvent>
#include <QtDebug>

GaloshWindow::GaloshWindow(QWidget* parent)
: QMainWindow(parent)
{
  QSplitter* splitter = new QSplitter(this);
  setCentralWidget(splitter);

  QSplitter* vsplit = new QSplitter(Qt::Vertical, this);
  splitter->addWidget(vsplit);

  roomView = new RoomView(vsplit);
  vsplit->addWidget(roomView);

  term = new GaloshTerm(vsplit);
  vsplit->addWidget(term);

  infoModel = new InfoModel(this);
  QObject::connect(term->socket(), SIGNAL(gmcpEvent(QString, QVariant)), this, SLOT(gmcpEvent(QString, QVariant)));

  infoView = new QTreeView(splitter);
  infoView->setModel(infoModel);
  infoView->setItemsExpandable(false);
  splitter->addWidget(infoView);

  QToolBar* tb = new QToolBar(this);
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
  vsplit->setSizes({ height() * 0.1, height() * 0.9 });
  splitter->setSizes({ width() * 0.75, width() * 0.25 });
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
}

void GaloshWindow::showEvent(QShowEvent* event)
{
  QMainWindow::showEvent(event);
  fixGeometry = true;
}

void GaloshWindow::paintEvent(QPaintEvent* event)
{
  if (fixGeometry) {
    if (pos().x() < 0 || pos().y() < 0) {
      move(0, 0);
    }
    fixGeometry = false;
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
