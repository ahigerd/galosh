#include "galoshwindow.h"
#include "galoshterm.h"
#include "telnetsocket.h"
#include "infomodel.h"
#include "roomview.h"
#include "profiledialog.h"
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
  tb->addAction("Connect", [this]{ openProfileDialog(true); });
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
  QObject::connect(term, SIGNAL(lineReceived(QString)), &triggers, SLOT(processLine(QString)));

  QObject::connect(term->socket(), SIGNAL(connected()), this, SLOT(updateStatus()));
  QObject::connect(term->socket(), SIGNAL(disconnected()), this, SLOT(updateStatus()));
  QObject::connect(term->socket(), SIGNAL(msspEvent(QString, QString)), &map, SLOT(msspEvent(QString, QString)));
  QObject::connect(term->socket(), SIGNAL(gmcpEvent(QString, QVariant)), &map, SLOT(gmcpEvent(QString, QVariant)));

  QObject::connect(&triggers, SIGNAL(executeCommand(QString, bool)), term, SLOT(executeCommand(QString, bool)), Qt::QueuedConnection);

  QObject::connect(&map, SIGNAL(currentRoomUpdated(MapManager*, int)), roomView, SLOT(setRoom(MapManager*, int)));
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
  qDebug() << key << value;
  if (key.toUpper() == "CHAR") {
    infoModel->loadTree(value);
    infoView->expandAll();
  }
}

void GaloshWindow::openProfileDialog(bool forConnect)
{
  ProfileDialog* dlg = new ProfileDialog(forConnect, this);
  if (forConnect) {
    QObject::connect(dlg, SIGNAL(connectToProfile(QString)), this, SLOT(connectToProfile(QString)));
  }
  dlg->open();
}

void GaloshWindow::connectToProfile(const QString& path)
{
  triggers.loadProfile(path);
  QSettings settings(path, QSettings::IniFormat);
  settings.beginGroup("Profile");
  term->socket()->connectToHost(settings.value("host").toString(), settings.value("port").toInt());
}
