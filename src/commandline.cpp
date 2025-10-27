#include "commandline.h"
#include <QAbstractItemView>
#include <QCompleter>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QScrollBar>
#include <QtDebug>

CommandLine::CommandLine(QWidget* parent)
: QLineEdit(parent), historyLimit(30), historyIndex(-1)
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
  static QRegularExpression wordChars("[^A-Za-z0-9_-]+");
  QStringList words = QString(line).replace(wordChars, " ").split(' ');
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
  QString line = text();
  emit commandEntered(line, echoMode() == QLineEdit::Normal);
  selectAll();
  history.removeAll(line);
  history.append(line);
  historyIndex = -1;
  if (history.length() > historyLimit) {
    history.removeFirst();
  }
  onLineReceived(line);
}

void CommandLine::keyPressEvent(QKeyEvent* event)
{
  int oldIndex = historyIndex;
  if (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab) {
    checkCompletion(event->key() == Qt::Key_Backtab ? -1 : 1);
    return;
  } else if (event->key() == Qt::Key_Right && completionVisible()) {
    checkCompletion(0, true);
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
  if (((oldIndex == 0 && historyIndex == -1) ||
       (oldIndex == -1 && historyIndex == 0)) &&
      text() == history[history.size() - 1]) {
    // If the current line hasn't been edited, it isn't different from the last history entry, so skip it
    historyIndex += (oldIndex == -1) ? 1 : -1;
  }
  if (historyIndex < -1) {
    historyIndex = -1;
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
    if (line[i] == ' ') {
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
