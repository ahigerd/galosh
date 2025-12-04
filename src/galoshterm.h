#ifndef GALOSH_GALOSHTERM_H
#define GALOSH_GALOSHTERM_H

#include <QWidget>
#include <QAbstractSocket>
#include "Vt102Emulation.h"
#include "colorschemes.h"
class QLabel;
class QScrollBar;
class QStackedWidget;
class QSplitter;
class QTimer;
class QToolButton;
class CommandLine;
class MultiCommandLine;
class TermSocket;
class TelnetSocket;

namespace Konsole {
  class TerminalDisplay;
  class ScreenWindow;
}

class GaloshTerm : public QWidget
{
Q_OBJECT
public:
  GaloshTerm(QWidget* parent = nullptr);

  bool eventFilter(QObject* obj, QEvent* event);

  inline TelnetSocket* socket() { return tel; }

  void writeColorLine(const QByteArray& colorCode, const QByteArray& message);

signals:
  void lineReceived(const QString& line);
  void commandEntered(const QString& command, bool echo = true);
  void slashCommand(const QString& command, const QStringList& args);
  void speedwalk(const QStringList& steps);

public slots:
  void setTermFont(const QFont& font);
  void setColorScheme(const ColorScheme& scheme);
  void showError(const QString& message);
  void processCommand(const QString& command, bool echo = true);

private slots:
  void executeCommand(const QString& command, bool echo = true);
  void onConnected();
  void onDisconnected();
  void onSocketError(QAbstractSocket::SocketError err);
  void onEchoChanged(bool on);
  void onReadyRead();
  void scheduleResizeAndScroll(bool scroll);
  void resizeAndScroll();
  void onSlashCommand(const QString& command, const QStringList& args);
  void openMultiline(bool on = true);

private:
  QTimer refreshThrottle;
  Konsole::Vt102Emulation vt102;
  Konsole::TerminalDisplay* term;
  Konsole::ScreenWindow* screen;
  TelnetSocket* tel;
  QSplitter* splitter;
  QStackedWidget* lineStack;
  CommandLine* line;
  MultiCommandLine* multiline;
  QLabel* multilineStatus;
  QToolButton* bMultiline;
  QScrollBar* scrollBar;
  bool pendingScroll;
  bool darkBackground;
};

#endif
