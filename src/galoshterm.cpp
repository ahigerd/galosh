#include "galoshterm.h"
#include "termsocket.h"
#include "telnetsocket.h"
#include "infomodel.h"
#include "qtermwidget.h"
#include "commandline.h"
#include <QFontDatabase>
#include <QVBoxLayout>
#include <QShortcut>

GaloshTerm::GaloshTerm(QWidget* parent)
: QWidget(parent)
{
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  term = new QTermWidget(false, this);
  term->setTerminalFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  term->setColorScheme("GreenOnBlack");
  term->setKeyBindings("linux");
  term->setKeyboardCursorShape(QTermWidget::KeyboardCursorShape(-1)); // intentionally invalid to disable cursor rendering
  layout->addWidget(term);
  new QShortcut(QKeySequence::Copy, term, SLOT(copyClipboard()), nullptr, Qt::WidgetWithChildrenShortcut);

  ts = new TermSocket(term->getPtySlaveFd(), this);
  QObject::connect(ts, SIGNAL(lineReceived(QString)), this, SIGNAL(lineReceived(QString)));

  tel = new TelnetSocket(this);
  QObject::connect(tel, SIGNAL(echoChanged(bool)), this, SLOT(onEchoChanged(bool)));
  QObject::connect(tel, SIGNAL(connected()), this, SLOT(onConnected()));
  QObject::connect(tel, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
  ts->bindDevice(tel);

  line = new CommandLine(this);
  QObject::connect(ts, SIGNAL(lineReceived(QString)), line, SLOT(onLineReceived(QString)));
  QObject::connect(line, SIGNAL(commandEntered(QString, bool)), this, SLOT(executeCommand(QString, bool)));
  layout->addWidget(line);

  //tel->connectToHost("fierymud.org", 4000);
  tel->connectToHost("localhost", 9999);

  line->setFocus();

  term->installEventFilter(this);
  for (QObject* child : term->children()) {
    if (dynamic_cast<QWidget*>(child)) {
      child->installEventFilter(this);
    }
  }

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
      payload.removeLast();
    } else if (payload.endsWith('\\')) {
      payload[payload.length() - 1] = '|';
      continue;
    }
    if (payload.isEmpty()) {
      ts->write("\r\n");
    } else if (!echo) {
      ts->write("\x1b[93m" + QByteArray(command.length(), '*') + "\x1b[39m\r\n");
    } else {
      ts->write("\x1b[93m" + payload + "\x1b[39m\r\n");
    }
    tel->write(payload + "\r\n");
    payload.clear();
  }
}

bool GaloshTerm::eventFilter(QObject* obj, QEvent* event)
{
  if (event->type() == QEvent::KeyPress) {
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
  ts->write("\x1b[1;4;32m* Connected to " + host.toUtf8() + "\x1b[0m\r\n\r\n");
}

void GaloshTerm::onDisconnected()
{
  ts->write("\r\n\x1b[1;4;31m* Disconnected\x1b[0m\r\n");
}
