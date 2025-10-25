#ifndef GALOSH_GALOSHTERM_H
#define GALOSH_GALOSHTERM_H

#include <QWidget>
#include <QAbstractSocket>
class QTermWidget;
class CommandLine;
class TermSocket;
class TelnetSocket;

class GaloshTerm : public QWidget
{
Q_OBJECT
public:
  GaloshTerm(QWidget* parent = nullptr);

  bool eventFilter(QObject* obj, QEvent* event);
  inline TelnetSocket* socket() { return tel; }

signals:
  void lineReceived(const QString& line);

public slots:
  void executeCommand(const QString& command, bool echo = true);

private slots:
  void onConnected();
  void onDisconnected();
  void onSocketError(QAbstractSocket::SocketError err);
  void onEchoChanged(bool on);

private:
  QTermWidget* term;
  TermSocket* ts;
  TelnetSocket* tel;
  CommandLine* line;
};

#endif
