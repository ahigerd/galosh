#ifndef GALOSH_MSSPVIEW_H
#define GALOSH_MSSPVIEW_H

#include <QDialog>
#include <QList>
#include <QPair>
#include <QTimer>
#include <QPointer>
#include <QAbstractSocket>
class TelnetSocket;
class QLabel;
class QFormLayout;

class MsspView : public QDialog
{
Q_OBJECT
public:
  MsspView(const QString& hostname, quint16 port, QWidget* parent = nullptr);
  MsspView(TelnetSocket* socket, QWidget* parent = nullptr);

public slots:
  void updateMssp();

private slots:
  void msspRequest();
  void onReadyRead();
  void lineReceived(const QString& line);
  void showError(QAbstractSocket::SocketError err = QAbstractSocket::SocketTimeoutError);

protected:
  void resizeEvent(QResizeEvent*);

private:
  QPair<QLabel*, QLabel*> getPair(const QString& key);
  void showMessage(const QString& msg);

  QList<QPair<QLabel*, QLabel*>> labels;
  QFormLayout* form;
  QPointer<TelnetSocket> socket;
  QTimer refreshThrottle;
  bool isManual;
  bool gotManualPreamble;
};

#endif
