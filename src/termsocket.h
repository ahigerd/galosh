#ifndef GALOSH_TERMSOCKET_H
#define GALOSH_TERMSOCKET_H

#include <QAbstractSocket>
#include <QTimer>

class TermSocket : public QAbstractSocket
{
Q_OBJECT
public:
  static QString stripVT100(const QByteArray& payload);

  TermSocket(int fd, QObject* parent = nullptr);

  void bindDevice(QIODevice* device);

signals:
  void lineReceived(const QString& line);

private slots:
  void devReadyRead();
  void thisReadyRead();
  void checkForPrompts();

private:
  QIODevice* boundDevice;
  QByteArray lineBuffer;
  QTimer lineTimer;
};

#endif
