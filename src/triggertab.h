#ifndef GALOSH_TRIGGERTAB_H
#define GALOSH_TRIGGERTAB_H

#include <QWidget>
#include "triggermanager.h"
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QCheckBox;
class QPushButton;

class TriggerTab : public QWidget
{
Q_OBJECT
public:
  TriggerTab(QWidget* parent = nullptr);

  void load(const QString& profile);
  bool save(const QString& profile);

signals:
  void markDirty();

private slots:
  void selectTrigger(QTreeWidgetItem* item);
  void newTrigger();
  void deleteTrigger();
  void updateTrigger();

private:
  QTreeWidgetItem* addItem(const TriggerDefinition& def);

  TriggerManager manager;
  QTreeWidget* list;
  QLineEdit* pattern;
  QLineEdit* command;
  QCheckBox* enable;
  QPushButton* bDelete;
};

#endif
