#ifndef GALOSH_COMMANDLINE_H
#define GALOSH_COMMANDLINE_H

#include <QLineEdit>
#include <QStringListModel>
#include <QSet>
class QCompleter;

class CommandLine : public QLineEdit
{
Q_OBJECT
public:
  CommandLine(QWidget* parent = nullptr);

  inline int historySize() const { return historyLimit; }
  void setHistorySize(int size);

signals:
  void commandEntered(const QString& command, bool echo);

public slots:
  void onLineReceived(const QString& line);

private slots:
  void onReturnPressed();

protected:
  bool focusNextPrevChild(bool) override;
  void keyPressEvent(QKeyEvent* event) override;

private:
  bool completionVisible() const;
  void checkCompletion(int move = 0, bool completeNow = false);
  void cancelCompletion();

  QStringList history;
  QString pendingLine;
  int historyLimit;
  int historyIndex;

  QSet<QString> knownWords;
  QStringListModel completionModel;
  QCompleter* completer;
};

#endif
