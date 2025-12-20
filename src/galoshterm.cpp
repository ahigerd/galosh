#include "galoshterm.h"
#include "telnetsocket.h"
#include "infomodel.h"
#include "commandline.h"
#include "multicommandline.h"
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
#include <QSplitter>
#include <QStackedWidget>
#include <QLabel>
#include <QToolButton>
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

  splitter = new QSplitter(Qt::Vertical, this);
  layout->addWidget(splitter, 1);

  QFrame* frame = new QFrame(splitter);
  frame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  splitter->addWidget(frame);

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

  hLayout->addWidget(term, 1);
  new QShortcut(QKeySequence::Copy, term, SLOT(copyClipboard()), nullptr, Qt::WidgetWithChildrenShortcut);

  scrollBar = new QScrollBar(Qt::Vertical, frame);
  QObject::connect(scrollBar, SIGNAL(valueChanged(int)), term->scrollBar(), SLOT(setValue(int)));
  QObject::connect(term->scrollBar(), SIGNAL(valueChanged(int)), scrollBar, SLOT(setValue(int)));
  QObject::connect(term->scrollBar(), SIGNAL(rangeChanged(int, int)), scrollBar, SLOT(setRange(int, int)));
  scrollBar->setRange(term->scrollBar()->minimum(), term->scrollBar()->maximum());
  scrollBar->setValue(term->scrollBar()->value());
  hLayout->addWidget(scrollBar, 0);

  multiline = new MultiCommandLine(splitter);
  multiline->setVisible(false);
  QObject::connect(multiline, SIGNAL(toggleMultiline(bool)), this, SLOT(openMultiline(bool)));
  splitter->addWidget(multiline);

  QHBoxLayout* lBar = new QHBoxLayout;
  lBar->setContentsMargins(0, 0, 0, 0);
  lBar->setSpacing(0);
  layout->addLayout(lBar, 0);

  lineStack = new QStackedWidget;
  lBar->addWidget(lineStack, 1);

  line = new CommandLine(lineStack);
  QObject::connect(tel, SIGNAL(lineReceived(QString)), line, SLOT(onLineReceived(QString)));
  QObject::connect(line, &CommandLine::commandEntered, [this](const QString&, bool) { screen->clearSelection(); });
  QObject::connect(line, SIGNAL(commandEntered(QString, bool)), this, SLOT(executeCommand(QString, bool)));
  QObject::connect(line, SIGNAL(commandEntered(QString, bool)), this, SIGNAL(commandEntered(QString, bool)));
  QObject::connect(line, SIGNAL(commandEnteredForProfile(QString, QString)), this, SIGNAL(commandEnteredForProfile(QString, QString)));
  QObject::connect(line, SIGNAL(showError(QString)), this, SLOT(showError(QString)));
  QObject::connect(line, SIGNAL(slashCommand(QString, QStringList)), this, SLOT(onSlashCommand(QString, QStringList)));
  QObject::connect(line, SIGNAL(speedwalk(QStringList)), this, SIGNAL(speedwalk(QStringList)));
  QObject::connect(line, SIGNAL(multilineRequested()), this, SLOT(openMultiline()));
  lineStack->addWidget(line);

  multilineStatus = new QLabel(lineStack);
  multilineStatus->setText(multiline->statusMessage());
  QObject::connect(multiline, SIGNAL(statusUpdated(QString)), multilineStatus, SLOT(setText(QString)));
  QObject::connect(multiline, SIGNAL(commandEntered(QString, bool)), line, SLOT(processCommand(QString, bool)));
  lineStack->addWidget(multilineStatus);

  bParse = new QToolButton(this);
  bParse->setText("\uFF0F");
  bParse->setCheckable(true);
  bParse->setChecked(true);
  bParse->setToolTip("Command Parsing");
  QObject::connect(bParse, SIGNAL(toggled(bool)), this, SLOT(setParsing(bool)));
  lBar->addWidget(bParse, 0, Qt::AlignBottom);

  bMultiline = new QToolButton(this);
  bMultiline->setText("\u2026");
  bMultiline->setCheckable(true);
  bMultiline->setChecked(false);
  bMultiline->setToolTip("Multiline Editor");
  QObject::connect(bMultiline, SIGNAL(toggled(bool)), this, SLOT(openMultiline(bool)));
  lBar->addWidget(bMultiline, 0, Qt::AlignBottom);

  term->installEventFilter(this);
  line->installEventFilter(this);

  refreshThrottle.setSingleShot(true);
  refreshThrottle.setInterval(5);
  QObject::connect(&refreshThrottle, SIGNAL(timeout()), this, SLOT(resizeAndScroll()));

  setTermFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  setColorScheme(ColorSchemes::scheme("Galosh Green"));
  openMultiline(false);
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

void GaloshTerm::openMultiline(bool on)
{
  bMultiline->blockSignals(true);
  bMultiline->setChecked(on);
  bMultiline->blockSignals(false);
  multiline->setVisible(on);
  if (on) {
    splitter->setSizes({ int(height() * 0.8), int(height() * 0.2) });
    QString content = line->text();
    if (line->isParsing()) {
      QStringList lines = CommandLine::parseMultilineCommand(content);
      content = lines.join("\n");
    }
    multiline->setPlainText(content);
    lineStack->setCurrentWidget(multilineStatus);
    setFocusProxy(multiline);
    multiline->setFocus();
  } else {
    QString content = multiline->toPlainText().replace("|", "\\|").replace("\n", "|");
    line->setText(content);
    lineStack->setCurrentWidget(line);
    setFocusProxy(line);
    line->setFocus();
  }
}

void GaloshTerm::processCommand(const QString& command, bool echo)
{
  line->processCommand(command, echo);
}

void GaloshTerm::executeCommand(const QString& command, bool echo)
{
  QByteArray payload = command.toUtf8();
  writeColorLine("93", echo ? payload : QByteArray(command.length(), '*'));
  if (!commandFilter || !commandFilter(command)) {
    if (tel->isConnected()) {
      tel->write(payload + "\r\n");
    }
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
  multiline->setFont(font);
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

bool GaloshTerm::isParsing() const
{
  return line->isParsing();
}

void GaloshTerm::setParsing(bool on)
{
  if (on != bParse->isChecked()) {
    line->setParsing(on);
    bParse->setChecked(on);
    emit parsingChanged(on);
  }
}

void GaloshTerm::setCommandFilter(std::function<bool(const QString&)> filter)
{
  commandFilter = filter;
}
