#include "commandline.h"
#include <QAbstractItemView>
#include <QCompleter>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QScrollBar>
#include <QtDebug>

static QRegularExpression nonWordChars("[^[:alnum:]_-]+", QRegularExpression::UseUnicodePropertiesOption);

QStringList CommandLine::parseSpeedwalk(const QString& dirs)
{
  if (dirs.isEmpty()) {
    // empty path
    return QStringList();
  }
  if (dirs.back() >= '0' && dirs.back() <= '9') {
    // invalid syntax
    return QStringList();
  }
  QStringList path;
  int count = -1;
  bool bracket = false;
  QString bracketed;
  for (QChar ch : dirs) {
    if (bracket) {
      if (ch == ']') {
        bracket = false;
        while (count-- > 0) {
          path << bracketed;
        }
        bracketed.clear();
      } else {
        bracketed += ch;
      }
    } else if (ch == '[') {
      bracket = true;
      if (count < 0) {
        count = 1;
      }
    } else if (ch >= '0' && ch <= '9') {
      if (count < 0) {
        count = 0;
      }
      count = count * 10 + (ch.cell() - '0');
    } else if (ch == '.' || ch == ' ') {
      // skip dots and spaces
    } else {
      if (count < 0) {
        count = 1;
      }
      while (count-- > 0) {
        path << QString(ch);
      }
    }
  }
  if (bracket) {
    // invalid syntax
    return QStringList();
  }
  return path;
}

QStringList CommandLine::parseSlashCommand(const QString& command)
{
  QStringList tokens = command.simplified().split(' ');
  if (tokens.isEmpty()) {
    tokens << "";
  } else {
    tokens[0] = tokens[0].toUpper();
  }
  return tokens;
}

QStringList CommandLine::parseMultilineCommand(const QString& command)
{
  QStringList result;
  QStringList lines = command.split('|');
  if (command.startsWith("//")) {
    lines[0] = lines[0].mid(1);
  }
  if (command.endsWith('\\')) {
    lines.last() += '\\';
  }
  QString partial;
  for (const QString& line : lines) {
    partial += line;
    if (partial.endsWith("\\\\")) {
      partial.chop(1);
    } else if (partial.endsWith('\\')) {
      partial[partial.length() - 1] = '|';
      continue;
    }
    result << partial;
    partial.clear();
  }
  if (!partial.isEmpty()) {
    qDebug() << "XXX: internal parsing error";
  }
  return result;
}

CommandLine::CommandLine(QWidget* parent)
: QLineEdit(parent), historyLimit(30), historyIndex(-1), parsing(true)
{
  completer = new QCompleter(&completionModel, this);
  completer->setCaseSensitivity(Qt::CaseInsensitive);
  completer->setWidget(this);

  QObject::connect(this, SIGNAL(returnPressed()), this, SLOT(onReturnPressed()));
}

bool CommandLine::focusNextPrevChild(bool)
{
  return false;
}

void CommandLine::setHistorySize(int size)
{
  historyLimit = size;
  if (size < history.length()) {
    history.erase(history.begin() + size, history.end());
  }
}

void CommandLine::onLineReceived(const QString& line)
{
  QStringList words = QString(line).replace(nonWordChars, " ").split(' ');
  for (const QString& word : words) {
    QString lower = word.toLower();
    if (word.length() < 4 || knownWords.contains(lower)) {
      continue;
    }
    int row = completionModel.rowCount();
    completionModel.insertRow(row);
    completionModel.setData(completionModel.index(row, 0), word);
    knownWords << lower;
  }
}

void CommandLine::onReturnPressed()
{
  if (completionVisible()) {
    checkCompletion(0, true);
    return;
  }
  bool echo = (echoMode() == QLineEdit::Normal);
  // pasting into a QLineEdit can inject invisible line breaks
  processCommand(text().replace("\r", "").replace("\n", " "), echo);
  selectAll();
  history.removeAll(text());
  if (echo) {
    // don't store passwords in command history
    history.append(text());
  }
  historyIndex = -1;
  if (history.length() > historyLimit) {
    history.removeFirst();
  }
}

void CommandLine::processCommand(const QString& command, bool echo)
{
  if (isParsing()) {
    if (command.startsWith('/') && command != "/" && !command.startsWith("//")) {
      QStringList args = parseSlashCommand(command.mid(1));
      QString slash = args.takeFirst();
      emit slashCommand(slash, args);
    } else if (command.startsWith('.') && command != ".") {
      // TODO: customizable speedwalk prefix
      QStringList dirs = parseSpeedwalk(command.mid(1));
      if (dirs.isEmpty()) {
        emit showError("Invalid speedwalk path");
      } else {
        emit speedwalk(dirs);
      }
    } else {
      QStringList lines = parseMultilineCommand(command);
      for (const QString& line : lines) {
        emit commandEntered(line, true);
        onLineReceived(line);
      }
    }
  } else {
    emit commandEntered(command, echo);
  }
}

void CommandLine::keyPressEvent(QKeyEvent* event)
{
  int oldIndex = historyIndex;
  if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && (event->modifiers() & Qt::ControlModifier)) {
    emit multilineRequested();
  } else if (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab) {
    if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
      event->ignore();
      return;
    }
    checkCompletion(event->key() == Qt::Key_Backtab ? -1 : 1);
    return;
  } else if (event->key() == Qt::Key_Right && completionVisible()) {
    checkCompletion(0, true);
    return;
  } else if ((event->modifiers() & Qt::ControlModifier) && (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete)) {
    if (hasSelectedText()) {
      insert("");
      event->ignore();
    } else {
      QLineEdit::keyPressEvent(event);
    }
    return;
  } else if (event->key() == Qt::Key_Down) {
    historyIndex--;
  } else if (event->key() == Qt::Key_Up) {
    historyIndex++;
  } else {
    QLineEdit::keyPressEvent(event);
    if (completionVisible()) {
      checkCompletion();
    }
    return;
  }
  if (history.isEmpty()) {
    historyIndex = -1;
    selectAll();
    return;
  }
  if (((oldIndex == 0 && historyIndex == -1) ||
       (oldIndex == -1 && historyIndex == 0)) &&
      text() == history[history.size() - 1]) {
    // If the current line hasn't been edited, it isn't different from the last history entry, so skip it
    historyIndex += (oldIndex == -1) ? 1 : -1;
  }
  if (historyIndex < -1) {
    historyIndex = -1;
    setText(pendingLine);
    selectAll();
    return;
  }
  if (historyIndex >= history.size()) {
    historyIndex = history.size() - 1;
    selectAll();
    return;
  }
  if (oldIndex == -1) {
    pendingLine = text();
  }
  if (historyIndex == -1) {
    setText(pendingLine);
  } else {
    setText(history[history.size() - historyIndex - 1]);
  }
  selectAll();
}

void CommandLine::focusInEvent(QFocusEvent* event)
{
  QLineEdit::focusInEvent(event);
  selectAll();
}

void CommandLine::cancelCompletion()
{
  if (completer->popup()) {
    completer->popup()->close();
  }
}

void CommandLine::checkCompletion(int move, bool completeNow)
{
  if (hasSelectedText()) {
    cancelCompletion();
    return;
  }
  QString line = text();
  int pos = cursorPosition();
  int wordStart = 0;
  for (int i = pos - 1; i >= 0; --i) {
    QChar ch = line[i];
    if (!ch.isLetterOrNumber() && ch != '_' && ch != '-') {
      wordStart = i + 1;
      break;
    }
  }
  int wordLen = pos - wordStart;
  if (wordLen < 1) {
    cancelCompletion();
    return;
  }
  QString word = line.mid(wordStart, wordLen);
  if (word != completer->completionPrefix()) {
    completer->setCompletionPrefix(word);
  }
  int count = completer->completionCount();
  if (!count) {
    return;
  }
  completeNow = completeNow || (count == 1 && move > 0);
  if (completeNow) {
    QString completion = completer->currentCompletion();
    line.replace(wordStart, wordLen, completion);
    setText(line);
    setCursorPosition(wordStart + completion.length());
    cancelCompletion();
    return;
  }
  auto popup = completer->popup();
  bool wasVisible = completionVisible();
  completer->complete(cursorRect());
  if (!popup) {
    popup = completer->popup();
  } else if (wasVisible && move) {
    int row = completer->currentRow();
    completer->setCurrentRow(row + move);
    if (completer->currentRow() == row) {
      completer->setCurrentRow(move < 0 ? count - 1 : 0);
    }
  }
  popup->setCurrentIndex(completer->currentIndex());
  int fw = popup->frameWidth() * 2;
  popup->resize(popup->sizeHintForColumn(0) + fw, popup->sizeHintForRow(0) * count + fw);
}

bool CommandLine::completionVisible() const
{
  return completer->popup() && completer->popup()->isVisible();
}

bool CommandLine::isParsing() const
{
  // Don't parse passwords
  return parsing && echoMode() == QLineEdit::Normal;
}

void CommandLine::setParsing(bool on)
{
  parsing = on;
}
