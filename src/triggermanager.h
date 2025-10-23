#ifndef GALOSH_TRIGGERMANAGER_H
#define GALOSH_TRIGGERMANAGER_H

#include <QObject>
#include <QList>
#include <QRegularExpression>

class TriggerManager : public QObject
{
Q_OBJECT
public:
  TriggerManager(QObject* parent = nullptr);

signals:
  void executeCommand(const QString& command, bool echo);

public slots:
  void loadProfile(const QString& profile);
  void processLine(const QString& line);

private:
  struct Trigger {
    QRegularExpression pattern;
    QString command;
    bool echo;
  };
  QList<Trigger> triggers;
};

#endif
