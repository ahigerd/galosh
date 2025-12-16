#ifndef GALOSH_COMMANDTAB_H
#define GALOSH_COMMANDTAB_H

#include "dialogtabbase.h"
#include "triggermanager.h"

class CommandTab : public DialogTabBase
{
Q_OBJECT
public:
  CommandTab(QWidget* parent = nullptr);

  virtual void load(UserProfile* profile) override;
  virtual bool save(UserProfile* profile) override;
};

#endif
