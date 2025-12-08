#ifndef GALOSH_SERVERTAB_H
#define GALOSH_SERVERTAB_H

#include "dialogtabbase.h"
class QLineEdit;
class QRadioButton;
class QLabel;
class QCheckBox;
class QPushButton;

class ServerTab : public DialogTabBase
{
Q_OBJECT
public:
  ServerTab(QWidget* parent = nullptr);

  QString profileName() const;

  void newProfile();
  virtual void load(UserProfile* profile) override;
  virtual bool save(UserProfile* profile) override;

private slots:
  void checkMssp();
  void toggleServerOrProgram();

private:
  QLineEdit* name;
  QRadioButton* oServer;
  QRadioButton* oProgram;
  QLineEdit* hostname;
  QLabel* hostLabel;
  QLineEdit* port;
  QCheckBox* useTls;
  QLabel* portLabel;
  QPushButton* msspButton;
  QLineEdit* username;
  QLineEdit* password;
  QLineEdit* loginPrompt;
  QLineEdit* passwordPrompt;
};

#endif
