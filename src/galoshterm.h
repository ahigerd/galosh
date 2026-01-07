#ifndef GALOSH_GALOSHTERM_H
#define GALOSH_GALOSHTERM_H

#include <QWidget>
#include <QAbstractSocket>
#include <functional>
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

  bool isParsing() const;

signals:
  void lineReceived(const QString& line);
  void commandEntered(const QString& command, bool echo = true);
  void commandsEntered(const QStringList& commands);
  bool parsingChanged(bool on);

public slots:
  void setTermFont(const QFont& font);
  void setColorScheme(const ColorScheme& scheme);
  void showSlashCommand(const QString& command, const QStringList& args);
  void showError(const QString& message);
  void processCommand(const QString& command, bool echo = true);
  void transmitCommand(const QString& command, bool echo = true);
  void setParsing(bool on);

private slots:
  void onConnected();
  void onDisconnected();
  void onSocketError(QAbstractSocket::SocketError err);
  void onEchoChanged(bool on);
  void onReadyRead();
  void scheduleResizeAndScroll(bool scroll);
  void resizeAndScroll();
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
  QToolButton* bParse;
  QScrollBar* scrollBar;
  bool pendingScroll;
  bool darkBackground;
};

#endif
