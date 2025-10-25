#ifndef GALOSH_TELNETSOCKET_H
#define GALOSH_TELNETSOCKET_H

#include <QTcpSocket>
#include <QMetaEnum>
#include <QHash>

class TelnetSocket : public QIODevice
{
Q_OBJECT
public:
  enum Telnet : quint8 {
    SE = 240,
    SB = 250,
    WILL = 251,
    WONT = 252,
    DO = 253,
    DONT = 254,
    IAC = 255,
  };
  Q_ENUM(Telnet);

  TelnetSocket(QObject* parent = nullptr);

  void connectToHost(const QString& host, quint16 port);
  void disconnectFromHost();
  bool isConnected() const;
  QString hostname() const;

  qint64 bytesAvailable() const override;
  qint64 bytesToWrite() const;

  QHash<QString, QString> mssp;
  QVariantMap gmcp;

signals:
  void connected();
  void disconnected();
  void errorOccurred(QAbstractSocket::SocketError);
  void echoChanged(bool on);
  void msspEvent(const QString& key, const QString& value);
  void gmcpEvent(const QString& key, const QVariant& value);

public slots:
  void consume();

protected:
  qint64 readData(char* data, qint64 maxSize) override;
  qint64 writeData(const char* data, qint64 maxSize) override;

private:
  void telnetDo(quint8 option, bool dont);
  void telnetWill(quint8 option, bool wont);
  void telnetSB(quint8 option, const QByteArray& payload);

  QTcpSocket* tcp;
  QByteArray protocolBuffer;
  QByteArray outputBuffer;
  QString connectedHost;
};

using Telnet = TelnetSocket::Telnet;

#endif
