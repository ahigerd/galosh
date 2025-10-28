#include "galoshterm.h"
#include "telnetsocket.h"
#include "infomodel.h"
#include "commandline.h"
#include "TerminalDisplay.h"
#include "ScreenWindow.h"
#include "Vt102Emulation.h"
#include "History.h"
#include <QEvent>
#include <QKeyEvent>
#include <QMetaEnum>
#include <QFontDatabase>
#include <QVBoxLayout>
#include <QShortcut>

using namespace Konsole;

GaloshTerm::GaloshTerm(QWidget* parent)
: QWidget(parent), pendingScroll(false)
{
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  vt102.setHistory(HistoryTypeBuffer(1000));
  screen = vt102.createWindow();
  term = new TerminalDisplay(this);
  term->setScreenWindow(screen);
  term->setBellMode(TerminalDisplay::NotifyBell);
  term->setTerminalSizeHint(true);
  term->setTripleClickMode(TerminalDisplay::SelectWholeLine);
  term->setTerminalSizeStartup(true);
  term->setVTFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  term->setKeyboardCursorShape(Emulation::KeyboardCursorShape::NoCursor);
  term->setBackgroundColor(Qt::black);
  term->setForegroundColor(QColor(24, 240, 24));
  layout->addWidget(term);
  new QShortcut(QKeySequence::Copy, term, SLOT(copyClipboard()), nullptr, Qt::WidgetWithChildrenShortcut);

  tel = new TelnetSocket(this);
  QObject::connect(tel, SIGNAL(echoChanged(bool)), this, SLOT(onEchoChanged(bool)));
  QObject::connect(tel, SIGNAL(connected()), this, SLOT(onConnected()));
  QObject::connect(tel, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
  QObject::connect(tel, SIGNAL(errorOccurred(QAbstractSocket::SocketError)), this, SLOT(onSocketError(QAbstractSocket::SocketError)));
  QObject::connect(tel, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
  QObject::connect(tel, SIGNAL(lineReceived(QString)), this, SIGNAL(lineReceived(QString)));

  line = new CommandLine(this);
  QObject::connect(tel, SIGNAL(lineReceived(QString)), line, SLOT(onLineReceived(QString)));
  QObject::connect(line, &CommandLine::commandEntered, [this](const QString&, bool) { screen->clearSelection(); });
  QObject::connect(line, SIGNAL(commandEntered(QString, bool)), this, SLOT(executeCommand(QString, bool)));
  layout->addWidget(line);

  line->setFocus();

  term->installEventFilter(this);

  refreshThrottle.setSingleShot(true);
  refreshThrottle.setInterval(5);
  QObject::connect(&refreshThrottle, SIGNAL(timeout()), this, SLOT(resizeAndScroll()));
}

void GaloshTerm::executeCommand(const QString& command, bool echo)
{
  QStringList lines = command.split('|');
  if (command.endsWith('\\')) {
    lines.last() += '\\';
  }
  QByteArray payload;
  for (const QString& line : lines) {
    payload += line.toUtf8();
    if (payload.endsWith("\\\\")) {
      payload.chop(1);
    } else if (payload.endsWith('\\')) {
      payload[payload.length() - 1] = '|';
      continue;
    }
    writeColorLine("93", echo ? payload : QByteArray(command.length(), '*'));
    tel->write(payload + "\r\n");
    payload.clear();
  }
}

bool GaloshTerm::eventFilter(QObject* obj, QEvent* event)
{
  if (event->type() == QEvent::Resize) {
    scheduleResizeAndScroll(true);
  } else if (event->type() == QEvent::KeyPress) {
    QKeyEvent* ke = static_cast<QKeyEvent*>(event);
    if (!ke->text().isEmpty()) {
      line->setFocus();
      line->event(event);
    }
    return true;
  }
  return QWidget::eventFilter(obj, event);
}

void GaloshTerm::onEchoChanged(bool on)
{
  QLineEdit::EchoMode mode = on ? QLineEdit::Normal : QLineEdit::Password;
  if (mode != line->echoMode()) {
    line->setEchoMode(mode);
    line->clear();
  }
}

void GaloshTerm::onConnected()
{
  QString host = tel->hostname();
  writeColorLine("1;4;32", "* Connected to " + host.toUtf8());
  writeColorLine("", "");
}

void GaloshTerm::onDisconnected()
{
  writeColorLine("", "");
  writeColorLine("1;4;31", "* Disconnected");
}

void GaloshTerm::onSocketError(QAbstractSocket::SocketError err)
{
  if (err == QAbstractSocket::RemoteHostClosedError) {
    // Normal operation
    return;
  }
  QByteArray name = QMetaEnum::fromType<QAbstractSocket::SocketError>().valueToKey(err);
  name = name.replace("Error", "");
  QByteArray msg = "Error:";
  int words = 1;
  for (char ch : name) {
    if (ch >= 'A' && ch <= 'Z') {
      ++words;
      msg += ' ';
    }
    msg += ch;
  }
  if (words < 3) {
    msg += " Error";
  }
  writeColorLine("1;4;31", msg);
}

void GaloshTerm::onReadyRead()
{
  QByteArray data = tel->read(tel->bytesAvailable());
  data = data.replace("\n", "\r\n").replace("\r\r", "\r");
  vt102.receiveData(data.constData(), data.length());
  scheduleResizeAndScroll(false);
}

void GaloshTerm::writeColorLine(const QByteArray& colorCode, const QByteArray& message)
{
  QByteArray payload;
  if (!colorCode.isEmpty() && !message.isEmpty()) {
    payload = "\x1b[" + colorCode + "m" + message + "\x1b[0m\r\n";
  } else {
    payload = message + "\r\n";
  }
  vt102.receiveData(payload.constData(), payload.length());
  scheduleResizeAndScroll(true);
}

void GaloshTerm::scheduleResizeAndScroll(bool scroll)
{
  if (scroll) {
    pendingScroll = true;
  }
  if (!refreshThrottle.isActive()) {
    refreshThrottle.start();
  }
}

void GaloshTerm::resizeAndScroll()
{
  QSize current = vt102.imageSize();
  if (term->lines() > 1 && term->columns() > 1 && term->lines() != current.height() && term->columns() != term->width()) {
    vt102.setImageSize(term->lines(), term->columns());
  }
  term->updateImage();
  if (pendingScroll) {
    term->scrollToEnd();
    pendingScroll = false;
  }
}
