#ifndef GALOSH_MULTICOMMANDLINE_H
#define GALOSH_MULTICOMMANDLINE_H

#include <QPlainTextEdit>
class QLabel;

class MultiCommandLine : public QPlainTextEdit
{
Q_OBJECT
public:
  MultiCommandLine(QWidget* parent = nullptr);

  QString statusMessage() const;

  QSize sizeHint() const;

signals:
  void statusUpdated(const QString& message);
  void toggleMultiline(bool on);
  void commandEntered(const QString& command, bool echo = true);

protected:
  void keyPressEvent(QKeyEvent* event);

private slots:
  void updateStatus();
};

#endif
