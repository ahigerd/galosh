#include "multicommandline.h"
#include <QBoxLayout>
#include <QFontMetrics>
#include <QStyle>

MultiCommandLine::MultiCommandLine(QWidget* parent)
: QPlainTextEdit(parent)
{
  QObject::connect(this, SIGNAL(blockCountChanged(int)), this, SLOT(updateStatus()));
}

QString MultiCommandLine::statusMessage() const
{
  QString message = tr(" Press Ctrl+Enter to send or Esc to close editor. Lines: %1");
  return message.arg(blockCount());
}

void MultiCommandLine::updateStatus()
{
  emit statusUpdated(statusMessage());
}

void MultiCommandLine::keyPressEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Escape && event->modifiers() == Qt::NoModifier) {
    emit toggleMultiline(false);
  } else if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && (event->modifiers() & Qt::ControlModifier)) {
    // TODO: option to change enter key behavior
    // TODO: option to not close multiline editor
    QString text = toPlainText();
    QStringList commands = text.isEmpty() ? QStringList() : text.split("\n");
    clear();
    emit toggleMultiline(false);
    // TODO: slow mode?
    for (const QString& command : commands) {
      emit commandEntered(command);
    }
  } else {
    QPlainTextEdit::keyPressEvent(event);
  }
}

QSize MultiCommandLine::sizeHint() const
{
  QFontMetrics fm(font());
  int lineHeight = fm.height();
  int height = lineHeight * 4;
  height += style()->pixelMetric(QStyle::PM_DefaultFrameWidth) * 2;
  return QSize(QPlainTextEdit::sizeHint().width(), height);
}
