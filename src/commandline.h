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
  static QStringList parseSpeedwalk(const QString& dirs);
  static QStringList parseSlashCommand(const QString& command);
  static QStringList parseMultilineCommand(const QString& command);

  CommandLine(QWidget* parent = nullptr);

  inline int historySize() const { return historyLimit; }
  void setHistorySize(int size);

  bool isParsing() const;

signals:
  void commandEntered(const QString& command, bool echo);
  void commandEnteredForProfile(const QString& profile, const QString& command);
  void showError(const QString& message);
  void slashCommand(const QString& command, const QStringList& args);
  void speedwalk(const QStringList& steps);
  void multilineRequested();

public slots:
  void setParsing(bool on);
  void onLineReceived(const QString& line);
  void processCommand(const QString& command, bool echo);

private slots:
  void onReturnPressed();

protected:
  bool focusNextPrevChild(bool) override;
  void keyPressEvent(QKeyEvent* event) override;
  void focusInEvent(QFocusEvent* event) override;

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
  bool parsing;
};

#endif
