#include "galoshterm.h"
#include "telnetsocket.h"
#include "infomodel.h"
#include "commandline.h"
#include "colorschemes.h"
#include "TerminalDisplay.h"
#include "ScreenWindow.h"
#include "Vt102Emulation.h"
#include "History.h"
#include <QEvent>
#include <QKeyEvent>
#include <QMetaEnum>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QShortcut>

using namespace Konsole;

GaloshTerm::GaloshTerm(QWidget* parent)
: QWidget(parent), pendingScroll(false)
{
  tel = new TelnetSocket(this);
  QObject::connect(tel, SIGNAL(echoChanged(bool)), this, SLOT(onEchoChanged(bool)));
  QObject::connect(tel, SIGNAL(connected()), this, SLOT(onConnected()));
  QObject::connect(tel, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
  QObject::connect(tel, SIGNAL(errorOccurred(QAbstractSocket::SocketError)), this, SLOT(onSocketError(QAbstractSocket::SocketError)));
  QObject::connect(tel, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
  QObject::connect(tel, SIGNAL(lineReceived(QString)), this, SIGNAL(lineReceived(QString)));

  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  QFrame* frame = new QFrame(this);
  frame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  layout->addWidget(frame, 1);

  QHBoxLayout* hLayout = new QHBoxLayout(frame);
  hLayout->setContentsMargins(0, 1, 1, 1);
  hLayout->setSpacing(2);

  vt102.setHistory(HistoryTypeBuffer(1000));
  screen = vt102.createWindow();
  term = new TerminalDisplay(frame);
  term->setBackgroundRole(QPalette::Window);
  term->setScreenWindow(screen);
  term->setBellMode(TerminalDisplay::NotifyBell);
  term->setTerminalSizeHint(true);
  term->setTripleClickMode(TerminalDisplay::SelectWholeLine);
  term->setTerminalSizeStartup(true);
  term->setKeyboardCursorShape(Emulation::KeyboardCursorShape::NoCursor);
  setTermFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  setColorScheme(ColorSchemes::scheme("Galosh Green"));

  hLayout->addWidget(term, 1);
  new QShortcut(QKeySequence::Copy, term, SLOT(copyClipboard()), nullptr, Qt::WidgetWithChildrenShortcut);

  scrollBar = new QScrollBar(Qt::Vertical, frame);
  QObject::connect(scrollBar, SIGNAL(valueChanged(int)), term->scrollBar(), SLOT(setValue(int)));
  QObject::connect(term->scrollBar(), SIGNAL(valueChanged(int)), scrollBar, SLOT(setValue(int)));
  QObject::connect(term->scrollBar(), SIGNAL(rangeChanged(int, int)), scrollBar, SLOT(setRange(int, int)));
  scrollBar->setRange(term->scrollBar()->minimum(), term->scrollBar()->maximum());
  scrollBar->setValue(term->scrollBar()->value());
  hLayout->addWidget(scrollBar, 0);

  line = new CommandLine(this);
  QObject::connect(tel, SIGNAL(lineReceived(QString)), line, SLOT(onLineReceived(QString)));
  QObject::connect(line, &CommandLine::commandEntered, [this](const QString&, bool) { screen->clearSelection(); });
  QObject::connect(line, SIGNAL(commandEntered(QString, bool)), this, SLOT(executeCommand(QString, bool)));
  QObject::connect(line, SIGNAL(commandEntered(QString, bool)), this, SIGNAL(commandEntered(QString, bool)));
  QObject::connect(line, SIGNAL(showError(QString)), this, SLOT(showError(QString)));
  QObject::connect(line, SIGNAL(slashCommand(QString, QStringList)), this, SLOT(onSlashCommand(QString, QStringList)));
  QObject::connect(line, SIGNAL(speedwalk(QStringList)), this, SIGNAL(speedwalk(QStringList)));
  layout->addWidget(line, 0);

  line->setFocus();

  term->installEventFilter(this);
  line->installEventFilter(this);

  refreshThrottle.setSingleShot(true);
  refreshThrottle.setInterval(5);
  QObject::connect(&refreshThrottle, SIGNAL(timeout()), this, SLOT(resizeAndScroll()));
}

void GaloshTerm::showError(const QString& message)
{
  writeColorLine("1;4;31", message.toUtf8());
}

void GaloshTerm::onSlashCommand(const QString& command, const QStringList& args)
{
  QString message = "/" + command + " " + args.join(" ");
  writeColorLine("1;96", message.toUtf8());
  emit slashCommand(command, args);
}

void GaloshTerm::processCommand(const QString& command, bool echo)
{
  line->processCommand(command, echo);
}

void GaloshTerm::executeCommand(const QString& command, bool echo)
{
  QByteArray payload = command.toUtf8();
  writeColorLine("93", echo ? payload : QByteArray(command.length(), '*'));
  if (tel->isConnected()) {
    tel->write(payload + "\r\n");
  }
}

bool GaloshTerm::eventFilter(QObject* obj, QEvent* event)
{
  if (obj == term && event->type() == QEvent::Resize) {
    scheduleResizeAndScroll(true);
  } else if (event->type() == QEvent::KeyPress) {
    QKeyEvent* ke = static_cast<QKeyEvent*>(event);
    if (ke->key() == Qt::Key_PageUp) {
      screen->handleCommandFromKeyboard(KeyboardTranslator::ScrollPageUpCommand);
    } else if (ke->key() == Qt::Key_PageDown) {
      screen->handleCommandFromKeyboard(KeyboardTranslator::ScrollPageDownCommand);
    } else if (obj == term) {
      if (ke->key() == Qt::Key_Home) {
        screen->handleCommandFromKeyboard(KeyboardTranslator::ScrollUpToTopCommand);
      } else if (ke->key() == Qt::Key_End) {
        screen->handleCommandFromKeyboard(KeyboardTranslator::ScrollDownToBottomCommand);
      } else if (!ke->text().isEmpty()) {
        line->setFocus();
        line->event(event);
      }
    } else {
      return false;
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
  QString msg = "Error:";
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
  showError(msg);
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

void GaloshTerm::setTermFont(const QFont& font)
{
  QFont bold = font;
  bold.setBold(true);
  QFontMetrics fm(font);
  QFontMetrics fmb(bold);

  term->setBoldIntense(fm.boundingRect("mmm").width() == fmb.boundingRect("mmm").width());
  term->setVTFont(font);
}

void GaloshTerm::setColorScheme(const ColorScheme& scheme)
{
  term->setColorTable(scheme.colors);
  darkBackground = scheme.isDarkBackground;
  /*
  // TODO: user-configurable
  QVector<ColorEntry> colors(TABLE_COLORS, ColorEntry());
  const ColorEntry* defaultTable = term->colorTable();
  for (int i = 0; i < TABLE_COLORS; i++) {
    colors[i] = defaultTable[i];
  }
  colors[0].color = QColor(24, 240, 24);
  colors[10].color = QColor(128, 255, 192);
  colors[1].color = QColor(32, 32, 32);
  colors[11].color = QColor(192, 255, 224);
  term->setColorTable(colors.constData());
  */
}
