#include "termsocket.h"
#include <QtDebug>

TermSocket::TermSocket(int fd, QObject* parent)
: QAbstractSocket(QAbstractSocket::TcpSocket, parent), boundDevice(nullptr)
{
  setSocketDescriptor(fd);
  connect(this, SIGNAL(readyRead()), this, SLOT(thisReadyRead()));
  connect(&lineTimer, SIGNAL(timeout()), this, SLOT(checkForPrompts()));
}

void TermSocket::bindDevice(QIODevice* device)
{
  if (boundDevice) {
    disconnect(this, 0, boundDevice, 0);
  }
  connect(device, SIGNAL(readyRead()), this, SLOT(devReadyRead()));
  boundDevice = device;
  if (bytesAvailable() > 0) {
    thisReadyRead();
  }
}

void TermSocket::devReadyRead()
{
  QByteArray payload = boundDevice->read(boundDevice->bytesAvailable());
  write(payload);
  lineBuffer += payload;
  if (lineBuffer.contains('\n')) {
    QByteArrayList lines = lineBuffer.replace("\r", "").split('\n');
    lineBuffer = lines.takeLast();
    for (const QByteArray& line : lines) {
      emit lineReceived(stripVT100(line));
    }
  }
  if (lineBuffer.isEmpty()) {
    lineTimer.stop();
  } else {
    lineTimer.start(100);
  }
}

void TermSocket::thisReadyRead()
{
  if (!boundDevice) {
    return;
  }
  QByteArray payload = read(bytesAvailable());
  boundDevice->write(payload);
}

void TermSocket::checkForPrompts()
{
  if (lineBuffer.isEmpty() || lineBuffer.contains('\n')) {
    // Either nothing to do, or devReadyRead will handle it
    return;
  }
  emit lineReceived(stripVT100(lineBuffer));
  lineBuffer.clear();
}

QString TermSocket::stripVT100(const QByteArray& payload)
{
  QByteArray result;
  int len = payload.size();
  bool escape = false;
  bool csi = false;
  for (int i = 0; i < len; i++) {
    char ch = payload[i];
    if (csi) {
      if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
        csi = false;
      }
    } else if (escape) {
      if (ch == '[') {
        csi = true;
      }
      escape = false;
    } else if (ch == '\x1b') {
      escape = true;
    } else {
      result += ch;
    }
  }
  return QString::fromUtf8(result);
}
