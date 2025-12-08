#ifndef GALOSH_TRIGGERTAB_H
#define GALOSH_TRIGGERTAB_H

#include <QWidget>
#include "dialogtabbase.h"
#include "triggermanager.h"
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QCheckBox;
class QPushButton;

class TriggerTab : public DialogTabBase
{
Q_OBJECT
public:
  TriggerTab(QWidget* parent = nullptr);

  virtual void load(UserProfile* profile) override;
  virtual bool save(UserProfile* profile) override;

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
