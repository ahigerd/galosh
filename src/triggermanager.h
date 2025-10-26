#ifndef GALOSH_TRIGGERMANAGER_H
#define GALOSH_TRIGGERMANAGER_H

#include <QObject>
#include <QList>
#include <QRegularExpression>
class QSettings;

struct TriggerDefinition
{
  explicit TriggerDefinition(const QString& id);
  TriggerDefinition(QSettings* profile, const QString& id);

  void save(QSettings* profile) const;
  void remove(QSettings* profile) const;

  bool isInternal() const;

  QString id;
  QRegularExpression pattern;
  QString command;
  bool echo;
  bool enabled;
  bool once;
  bool triggered;
};

class TriggerManager : public QObject
{
Q_OBJECT
public:
  TriggerManager(QObject* parent = nullptr);

  QList<TriggerDefinition> triggers;
  TriggerDefinition* findTrigger(const QString& id);

signals:
  void executeCommand(const QString& command, bool echo);

public slots:
  void loadProfile(const QString& profile);
  void saveProfile(const QString& profile);
  void processLine(const QString& line);
};

#endif
