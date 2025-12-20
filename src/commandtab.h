#ifndef GALOSH_COMMANDTAB_H
#define GALOSH_COMMANDTAB_H

#include "dialogtabbase.h"
#include "triggermanager.h"
class QVBoxLayout;
class QListWidget;
class QListWidgetItem;
class QToolButton;
class QLineEdit;

class CommandTab : public DialogTabBase
{
Q_OBJECT
public:
  CommandTab(QWidget* parent = nullptr);

  virtual void load(UserProfile* profile) override;
  virtual bool save(UserProfile* profile) override;

private slots:
  void addCommand();
  void removeCommands();
  void onCommandSelected(QListWidgetItem* item);

  void addAction();
  void removeAction();

  void onCommandChanged();
  void onActionChanged();

private:
  void clearCommand();

  struct Row {
    QLineEdit* line;
    QToolButton* remove;
  };

  QListWidget* lCommands;
  QLineEdit* eCommand;
  QList<Row> rows;
  QVBoxLayout* lActions;
};

#endif
