#include "servertab.h"
#include "msspview.h"
#include "userprofile.h"
#include <QSettings>
#include <QFormLayout>
#include <QBoxLayout>
#include <QLineEdit>
#include <QRadioButton>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QButtonGroup>
#include <QIntValidator>

static const char* defaultLoginPrompt = "By what name do you wish to be known?";
static const char* defaultPasswordPrompt = "Password:";

static QString cleanPattern(QString pattern)
{
  // By\ what\ name\ do\ you\ wish\ to\ be\ known\?
  static QRegularExpression clean(R"(\\(.))");
  return pattern.replace(clean, R"(\1)");
}

ServerTab::ServerTab(QWidget* parent)
: DialogTabBase(tr("Server"), parent)
{
  QFormLayout* lServer = new QFormLayout(this);

  lServer->addRow("Profile &name:", name = new QLineEdit(this));
  lServer->addRow(horizontalLine(this));

  QHBoxLayout* lRadio = new QHBoxLayout;
  lRadio->addWidget(oServer = new QRadioButton("Connect to a se&rver", this), 1);
  lRadio->addWidget(oProgram = new QRadioButton("Run a p&rogram", this), 1);
  QButtonGroup* group = new QButtonGroup(this);
  group->addButton(oServer, 0);
  group->addButton(oProgram, 1);
  oServer->setChecked(true);
  lServer->addRow(lRadio);
  QObject::connect(oServer, SIGNAL(clicked()), this, SLOT(toggleServerOrProgram()));
  QObject::connect(oProgram, SIGNAL(clicked()), this, SLOT(toggleServerOrProgram()));

  lServer->addRow(hostLabel = new QLabel("&Hostname:", this), hostname = new QLineEdit(this));
  hostLabel->setBuddy(hostname);
  QHBoxLayout* portLayout = new QHBoxLayout;
  portLayout->addWidget(port = new QLineEdit("4000", this), 1);
  msspButton = new QPushButton("&MSSP...", this);
  portLayout->addWidget(msspButton);
  lServer->addRow(portLabel = new QLabel("Por&t:", this), portLayout);
  portLabel->setBuddy(port);
  useTls = new QCheckBox("Use &SSL / TLS", this);
#ifndef QT_NO_SSL
  lServer->addRow("", useTls);
#endif

  lServer->addRow(horizontalLine(this));

  lServer->addRow("&Username:", username = new QLineEdit(this));
  lServer->addRow("&Password:", password = new QLineEdit(this));
  lServer->addRow("", new QLabel("<i>Note: passwords are saved in plaintext</i>"));
  lServer->addRow(horizontalLine(this));

  lServer->addRow("Us&ername prompt:", loginPrompt = new QLineEdit(defaultLoginPrompt, this));
  lServer->addRow("P&assword prompt:", passwordPrompt = new QLineEdit(defaultPasswordPrompt, this));
  setMinimumWidth(400);

  port->setValidator(new QIntValidator(1, 65535, port));
  password->setEchoMode(QLineEdit::Password);

  autoConnectMarkDirty();
  QObject::connect(msspButton, SIGNAL(clicked()), this, SLOT(checkMssp()));

  toggleServerOrProgram();
}

QString ServerTab::profileName() const
{
  return name->text().trimmed();
}

void ServerTab::checkMssp()
{
  (new MsspView(hostname->text(), port->text().toInt(), useTls->isChecked(), this))->open();
}

void ServerTab::toggleServerOrProgram()
{
  bool server = oServer->isChecked();
  hostLabel->setText(server ? "&Hostname:" : "Pro&gram:");
  port->setEnabled(server);
  useTls->setEnabled(server);
  portLabel->setEnabled(server);
  msspButton->setEnabled(server);
  markDirty();
}

void ServerTab::newProfile()
{
  name->clear();
  hostname->clear();
  port->setText("4000");
  useTls->setChecked(false);
  username->clear();
  password->clear();
  loginPrompt->setText(defaultLoginPrompt);
  passwordPrompt->setText(defaultPasswordPrompt);

  name->setFocus();
}

void ServerTab::load(UserProfile* profile)
{
  name->setText(profile->profileName);
  QString command = profile->command;
  if (command.isEmpty()) {
    oServer->setChecked(true);
    hostname->setText(profile->host);
    port->setText(QString::number(profile->port));
    useTls->setChecked(profile->tls);
  } else {
    oProgram->setChecked(true);
    hostname->setText(command);
    port->clear();
    useTls->setChecked(false);
  }
  username->setText(profile->username);
  password->setText(profile->password);

  TriggerDefinition* usernameTrigger = profile->triggers.findTrigger(TriggerManager::UsernameId);
  loginPrompt->setText(usernameTrigger ? cleanPattern(usernameTrigger->pattern.pattern()) : "");

  TriggerDefinition* passwordTrigger = profile->triggers.findTrigger(TriggerManager::PasswordId);
  passwordPrompt->setText(passwordTrigger ? cleanPattern(passwordTrigger->pattern.pattern()) : "");

  toggleServerOrProgram();
}

bool ServerTab::save(UserProfile* profile)
{
  profile->profileName = profileName();
  if (oServer->isChecked()) {
    profile->host = hostname->text().trimmed();
    profile->port = port->text().toInt();
    profile->tls = useTls->isChecked();
    profile->command.clear();
  } else {
    profile->command = hostname->text().trimmed();
  }
  profile->username = username->text().trimmed();
  profile->password = password->text();

  TriggerDefinition* usernameTrigger = profile->triggers.findTrigger(TriggerManager::UsernameId, true);
  usernameTrigger->pattern.setPattern(loginPrompt->text().trimmed());
  usernameTrigger->command = profile->username;
  usernameTrigger->once = true;

  TriggerDefinition* passwordTrigger = profile->triggers.findTrigger(TriggerManager::PasswordId, true);
  passwordTrigger->pattern.setPattern(passwordPrompt->text().trimmed());
  passwordTrigger->command = profile->password;
  passwordTrigger->once = true;
  passwordTrigger->echo = false;

  return true;
}
