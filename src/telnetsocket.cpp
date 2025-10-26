#include "telnetsocket.h"
#include <QJsonDocument>
#include <QtDebug>
#include <map>
#include <cstring>

QString TelnetSocket::stripVT100(const QByteArray& payload)
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

TelnetSocket::TelnetSocket(QObject* parent)
: QIODevice(parent)
{
  setOpenMode(QIODevice::ReadWrite);
  tcp = new QTcpSocket(this);
  QObject::connect(tcp, SIGNAL(connected()), this, SIGNAL(connected()));
  QObject::connect(tcp, SIGNAL(disconnected()), this, SIGNAL(disconnected()));
  QObject::connect(tcp, SIGNAL(errorOccurred(QAbstractSocket::SocketError)), this, SIGNAL(errorOccurred(QAbstractSocket::SocketError)));
  QObject::connect(tcp, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
  QObject::connect(&lineTimer, SIGNAL(timeout()), this, SLOT(checkForPrompts()));
}

QString TelnetSocket::hostname() const
{
  if (isConnected()) {
    return connectedHost;
  }
  return QString();
}

void TelnetSocket::connectToHost(const QString& host, quint16 port)
{
  connectedHost = host;
  tcp->connectToHost(host, port);
}

void TelnetSocket::disconnectFromHost()
{
  tcp->disconnectFromHost();
}

bool TelnetSocket::isConnected() const
{
  return tcp->state() != QAbstractSocket::UnconnectedState;
}

qint64 TelnetSocket::bytesAvailable() const
{
  return outputBuffer.size();
}

qint64 TelnetSocket::bytesToWrite() const
{
  return tcp->bytesToWrite();
}

void TelnetSocket::onReadyRead()
{
  protocolBuffer += tcp->read(tcp->bytesAvailable());
  qint64 len = protocolBuffer.size();
  if (!len) {
    return;
  }
  qint64 i = 0;
  const quint8* src = reinterpret_cast<const quint8*>(protocolBuffer.constData());
  bool didExtend = false;
  for (; i < len; i++) {
    if (src[i] == 0xFF) {
      if (i + 1 >= len) {
        break;
      }
      quint8 fn = src[i + 1];
      // qDebug() << "::: IAC" << QMetaEnum::fromType<Telnet>().valueToKey(fn);
      if (fn == Telnet::IAC) {
        didExtend = true;
        outputBuffer.append(Telnet::IAC);
      } else if (fn >= Telnet::WILL && fn <= Telnet::DONT) {
        if (i + 2 >= len) {
          break;
        }
        quint8 param = src[i + 2];
        if (fn == Telnet::WILL || fn == Telnet::WONT) {
          telnetWill(param, fn == Telnet::WONT);
        } else if (fn == Telnet::DO || fn == Telnet::DONT) {
          telnetDo(param, fn == Telnet::DONT);
        }
        i += 2;
      } else if (fn == Telnet::SB) {
        static const quint8 SE[] = { Telnet::IAC, Telnet::SE, 0 };
        qsizetype endPos = protocolBuffer.indexOf((const char*)SE, i); // look for SE
        if (endPos < 0) {
          // haven't received end message yet
          break;
        }
        telnetSB(src[i + 2], protocolBuffer.mid(i + 3, endPos - i - 3));
        i = endPos + 1;
      } else {
        i += 1;
      }
    } else {
      didExtend = true;
      outputBuffer.append(src[i]);
    }
  }
  protocolBuffer.remove(0, i);
  if (didExtend) {
    emit readyRead();
  }
}

qint64 TelnetSocket::readData(char* data, qint64 maxSize)
{
  int size = maxSize > outputBuffer.size() ? outputBuffer.size() : maxSize;
  std::memcpy(data, outputBuffer.constData(), size);
  outputBuffer.remove(0, size);

  lineBuffer += QByteArray::fromRawData(data, size);
  if (lineBuffer.contains('\n')) {
    QTimer::singleShot(0, this, SLOT(processLines()));
  } else {
    lineTimer.start(100);
  }

  return size;
}

qint64 TelnetSocket::writeData(const char* data, qint64 maxSize)
{
  // qDebug() << "<<<<<<" << QByteArray(data, maxSize).toHex();
  return tcp->write(data, maxSize);
}

void TelnetSocket::telnetDo(quint8 option, bool dont)
{
  // qDebug() << (dont ? ">>> don't" : ">>> do") << option;
  quint8 response[3] = { Telnet::IAC, Telnet::WONT, option };
  if (!dont) {
    if (option == 0x18) {
      // terminal type
      response[1] = Telnet::WILL;
    }
  }
  // qDebug() << "<<< IAC" << QMetaEnum::fromType<Telnet>().valueToKey(response[1]) << int(option);
  write(reinterpret_cast<char*>(response), 3);
}

void TelnetSocket::telnetWill(quint8 option, bool wont)
{
  // qDebug() << (wont ? ">>> won't" : ">>> will") << option;
  quint8 response[3] = { Telnet::IAC, wont ? Telnet::DONT : Telnet::DO, option };
  if (option == 1) {
    emit echoChanged(wont);
  }
  // qDebug() << "<<< IAC" << QMetaEnum::fromType<Telnet>().valueToKey(response[1]) << int(option);
  write(reinterpret_cast<char*>(response), 3);
}

void TelnetSocket::telnetSB(quint8 option, const QByteArray& payload)
{
  if (option == 24 && payload.size() == 1 && payload[0] == 0x01) {
    static const char termType[] = "\xff\xfa\x18\x00MUDLET\xff\xf0";
    write(QByteArray(termType, sizeof(termType)));
  } else if (option == 70) {
    QString key, value;
    for (int i = 1; i < payload.size(); i++) {
      qsizetype pos = payload.indexOf('\x02', i);
      if (pos < 0) {
        qDebug() << "XXX: Invalid MSSP payload";
        return;
      }
      key = QString::fromUtf8(payload.mid(i, pos - i));
      i = pos + 1;
      pos = payload.indexOf('\x01', i);
      if (pos < 0) {
        value = QString::fromUtf8(payload.mid(i));
        i = payload.size();
      } else {
        value = QString::fromUtf8(payload.mid(i, pos - i));
        i = pos;
      }
      mssp[key] = value;
      emit msspEvent(key, value);
    }
  } else if (option == 201) {
    QString key;
    QVariant value;
    qsizetype pos = payload.indexOf(' ');
    if (pos < 0) {
      key = QString::fromUtf8(payload);
    } else {
      key = QString::fromUtf8(payload.mid(0, pos));
      QJsonDocument json = QJsonDocument::fromJson(payload.mid(pos + 1));
      value = json.toVariant();
    }
    gmcp[key] = value;
    // qDebug() << key << value;
    emit gmcpEvent(key, value);
  } else {
    // qDebug() << "SB" << int(option) << payload.toHex() << payload;
  }
}

void TelnetSocket::processLines()
{
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

void TelnetSocket::checkForPrompts()
{
  if (lineBuffer.isEmpty() || lineBuffer.contains('\n')) {
    // Either nothing to do, or onReadyRead will handle it
    return;
  }
  emit lineReceived(stripVT100(lineBuffer));
  lineBuffer.clear();
}
