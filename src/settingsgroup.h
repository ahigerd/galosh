#ifndef GALOSH_SETTINGSGROUP_H
#define GALOSH_SETTINGSGROUP_H

#include <QSettings>

class SettingsGroup
{
public:
  inline SettingsGroup(QSettings* settings, const QString& group)
  : settings(settings)
  {
    settings->beginGroup(group);
  }

  inline ~SettingsGroup()
  {
    settings->endGroup();
  }

private:
  QSettings* settings;
};

#endif
