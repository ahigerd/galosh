#include "msspview.h"
#include "telnetsocket.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QtDebug>

MsspView::MsspView(const QString& hostname, quint16 port, bool tls, QWidget* parent)
: MsspView(nullptr, parent)
{
  if (hostname.isEmpty() || port < 1) {
    QMessageBox::critical(this, "Galosh", "Hostname and port are required.");
    close();
  }

  setWindowTitle(QStringLiteral("MSSP: %1").arg(hostname));
  isManual = true;

  socket = new TelnetSocket(this);
  QObject::connect(socket, SIGNAL(msspEvent(QString, QString)), &refreshThrottle, SLOT(start()));
  QObject::connect(socket, SIGNAL(errorOccurred(QAbstractSocket::SocketError)), this, SLOT(showError(QAbstractSocket::SocketError)));
  socket->connectToHost(hostname, port, tls);

  showMessage("Loading...");

  QTimer::singleShot(500, this, SLOT(msspRequest()));
}

MsspView::MsspView(TelnetSocket* socket, QWidget* parent)
: QDialog(parent), socket(socket), isManual(false)
{
  if (socket) {
    setWindowTitle(QStringLiteral("MSSP: %1").arg(socket->hostname()));
  }
  setAttribute(Qt::WA_DeleteOnClose, true);

  QVBoxLayout* layout = new QVBoxLayout(this);
  form = new QFormLayout;
  form->setSpacing(1);
  layout->addLayout(form, 1);

  QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
  layout->addWidget(buttons, 0);

  refreshThrottle.setSingleShot(true);
  refreshThrottle.setInterval(100);

  QObject::connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
  QObject::connect(&refreshThrottle, SIGNAL(timeout()), this, SLOT(updateMssp()));
  if (socket) {
    QObject::connect(socket, SIGNAL(msspEvent(QString, QString)), &refreshThrottle, SLOT(start()));
    QObject::connect(socket, SIGNAL(errorOccurred(QAbstractSocket::SocketError)), this, SLOT(showError(QAbstractSocket::SocketError)));
  }

  setMinimumWidth(300);

  updateMssp();
}

QPair<QLabel*, QLabel*> MsspView::getPair(const QString& key)
{
  for (const auto& pair : labels) {
    if (pair.first->text() == key) {
      return pair;
    }
  }

  QPair<QLabel*, QLabel*> pair;
  pair.first = new QLabel(key, this);
  pair.second = new QLabel(this);
  pair.second->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  pair.second->setTextInteractionFlags(Qt::TextBrowserInteraction);
  pair.second->setOpenExternalLinks(true);
  pair.second->setAutoFillBackground(true);
  pair.second->setBackgroundRole(QPalette::Base);
  pair.second->setMargin(3);
  form->addRow(pair.first, pair.second);
  labels << pair;
  // TODO: two-column view?
  return pair;
}

void MsspView::updateMssp()
{
  if (!socket) {
    return;
  }

  QSet<QString> unused;
  for (const auto& pair : labels) {
    unused << pair.first->text();
  }

  for (const QString& key : socket->mssp.keys()) {
    auto pair = getPair(key);
    QString value = socket->mssp[key].trimmed();
    if (value.contains("://")) {
      value = QStringLiteral("<a href='%1'>%1</a>").arg(value);
    }
    pair.second->setText(value);
    unused.remove(key);
  }

  for (const QString& key : unused) {
    auto pair = getPair(key);
    labels.removeAll(pair);
    form->removeRow(pair.second);
  }

  resize(minimumSizeHint());

  if (isManual) {
    socket->disconnectFromHost();
  }
}

void MsspView::showError(QAbstractSocket::SocketError err)
{
  if (labels.size() > 1) {
    // Whatever error may have arisen, we already got what we wanted
    return;
  }
  QString msg;
  switch (err) {
  case QAbstractSocket::ConnectionRefusedError:
  case QAbstractSocket::HostNotFoundError:
  case QAbstractSocket::SocketTimeoutError:
    msg = QStringLiteral("Unable to connect to %1:%2.").arg(socket->hostname()).arg(socket->port());
    break;
  default:
    msg = QStringLiteral("No MSSP information available for %1.").arg(socket->hostname());
  };
  showMessage(msg);
  QObject::disconnect(socket, 0, this, 0);
  socket->disconnectFromHost();
}

void MsspView::showMessage(const QString& msg)
{
  if (labels.size() == 1) {
    form->removeRow(labels.first().second);
    labels.clear();
  }

  QLabel* label = new QLabel(msg);
  label->setAlignment(Qt::AlignCenter);
  labels << qMakePair(label, label);
  form->addRow(label);

  resize(minimumSizeHint());
}

void MsspView::msspRequest()
{
  if (!socket || !socket->isConnected()) {
    // already disconnected
    return;
  }
  QObject::connect(socket, SIGNAL(lineReceived(QString)), this, SLOT(lineReceived(QString)));
  QObject::connect(socket, SIGNAL(disconnected()), this, SLOT(updateMssp()));
  QObject::connect(socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
  QTimer::singleShot(1000, this, SLOT(showError()));
  QTimer::singleShot(1000, socket, SLOT(disconnectFromHost()));
  socket->write("mssp-request\r\n");
  gotManualPreamble = false;
}

void MsspView::onReadyRead()
{
  // Consume all data to trigger internal parsing
  socket->read(socket->bytesAvailable());
}

void MsspView::lineReceived(const QString& line)
{
  if (!gotManualPreamble) {
    gotManualPreamble = (line == "MSSP-REPLY-START");
    return;
  }
  if (line == "MSSP-REPLY-END") {
    updateMssp();
    socket->disconnectFromHost();
    return;
  }
  QStringList parts = line.split("\t");
  if (parts.size() != 2) {
    return;
  }
  socket->mssp[parts[0]] = parts[1];
}

void MsspView::resizeEvent(QResizeEvent*)
{
  QRect rect = frameGeometry();
  QRect desktopRect = qApp->desktop()->availableGeometry(this);
  if (!desktopRect.contains(rect)) {
    QPoint target = pos();
    if (rect.top() < desktopRect.top()) {
      target.setY(desktopRect.top());
    } else if (rect.bottom() > desktopRect.bottom()) {
      target.setY(desktopRect.bottom() - rect.height());
    }
    if (rect.left() < desktopRect.left()) {
      target.setX(desktopRect.left());
    } else if (rect.right() > desktopRect.right()) {
      target.setX(desktopRect.right() - rect.width());
    }
    move(target);
  }
}
