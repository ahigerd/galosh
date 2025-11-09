#include "profiledialog.h"
#include "triggertab.h"
#include "msspview.h"
#include "telnetsocket.h"
#include <QMessageBox>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSettings>
#include <QTabWidget>
#include <QListView>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPushButton>
#include <QFormLayout>
#include <QGridLayout>
#include <QBoxLayout>
#include <QGroupBox>
#include <QToolButton>
#include <QLabel>
#include <QDir>
#include <QtDebug>

static const char* defaultLoginPrompt = "By what name do you wish to be known?";
static const char* defaultPasswordPrompt = "Password:";

static QFrame* horizontalLine(QWidget* parent)
{
  QFrame* line = new QFrame(parent);
  line->setFrameStyle(QFrame::HLine | QFrame::Plain);
  return line;
}

ProfileDialog::ProfileDialog(bool forConnection, QWidget* parent)
: QDialog(parent), emitConnect(forConnection), dirty(false)
{
  setWindowTitle("Profiles");

  QGridLayout* layout = new QGridLayout(this);

  QGroupBox* bList = new QGroupBox("&Profiles", this);
  QVBoxLayout* lList = new QVBoxLayout(bList);
  layout->addWidget(bList, 0, 0);

  knownProfiles = new QListView(this);
  profileList = new QStandardItemModel(this);
  knownProfiles->setModel(profileList);
  knownProfiles->setMinimumWidth(100);
  lList->addWidget(knownProfiles, 1);

  QHBoxLayout* lButtons = new QHBoxLayout;
  lButtons->addStretch(1);
  lList->addLayout(lButtons, 0);

  QToolButton* bAdd = new QToolButton(bList);
  bAdd->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
  QObject::connect(bAdd, SIGNAL(clicked()), this, SLOT(newProfile()));
  lButtons->addWidget(bAdd, 0);

  QToolButton* bDelete = new QToolButton(bList);
  bDelete->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
  QObject::connect(bDelete, SIGNAL(clicked()), this, SLOT(deleteProfile()));
  lButtons->addWidget(bDelete, 0);

  tabs = new QTabWidget(this);
  layout->addWidget(tabs, 0, 1);

  QWidget* tServer = new QWidget(tabs);
  QFormLayout* lServer = new QFormLayout(tServer);
  tabs->addTab(tServer, "&Server");
  tabs->addTab(tTriggers = new TriggerTab(tabs), "&Triggers");

  lServer->addRow("Profile &name:", profileName = new QLineEdit(tServer));
  lServer->addRow(horizontalLine(tServer));

  QHBoxLayout* lRadio = new QHBoxLayout;
  lRadio->addWidget(oServer = new QRadioButton("Connect to a &server", tServer), 1);
  lRadio->addWidget(oProgram = new QRadioButton("Run a p&rogram", tServer), 1);
  QButtonGroup* group = new QButtonGroup(this);
  group->addButton(oServer, 0);
  group->addButton(oProgram, 1);
  oServer->setChecked(true);
  lServer->addRow(lRadio);
  QObject::connect(oServer, SIGNAL(clicked()), this, SLOT(toggleServerOrProgram()));
  QObject::connect(oProgram, SIGNAL(clicked()), this, SLOT(toggleServerOrProgram()));

  lServer->addRow(hostLabel = new QLabel("&Hostname:", tServer), hostname = new QLineEdit(tServer));
  hostLabel->setBuddy(hostname);
  QHBoxLayout* portLayout = new QHBoxLayout;
  portLayout->addWidget(port = new QLineEdit("4000", tServer), 1);
  msspButton = new QPushButton("&MSSP...", this);
  portLayout->addWidget(msspButton);
  lServer->addRow(portLabel = new QLabel("Por&t:", tServer), portLayout);
  portLabel->setBuddy(port);

  //lServer->addRow("&Command:", commandLine = new QLineEdit(tServer));
  lServer->addRow(horizontalLine(tServer));

  lServer->addRow("&Username:", username = new QLineEdit(tServer));
  lServer->addRow("&Password:", password = new QLineEdit(tServer));
  lServer->addRow("", new QLabel("<i>Note: passwords are saved in plaintext</i>"));
  lServer->addRow(horizontalLine(tServer));

  lServer->addRow("Us&ername prompt:", loginPrompt = new QLineEdit(defaultLoginPrompt, tServer));
  lServer->addRow("P&assword prompt:", passwordPrompt = new QLineEdit(defaultPasswordPrompt, tServer));
  tServer->setMinimumWidth(400);

  port->setValidator(new QIntValidator(1, 65535, port));
  password->setEchoMode(QLineEdit::Password);

  buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Close, this);
  if (forConnection) {
    buttons->button(QDialogButtonBox::Ok)->setText("&Connect");
    QPushButton* offlineButton = buttons->addButton("&Offline", QDialogButtonBox::ActionRole);
    QObject::connect(offlineButton, SIGNAL(clicked()), this, SLOT(loadProfileOffline()));
  }
  QObject::connect(buttons, SIGNAL(accepted()), this, SLOT(saveAndClose()));
  QObject::connect(buttons, SIGNAL(rejected()), this, SLOT(closePromptUnsaved()));
  QObject::connect(buttons->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(save()));
  layout->addWidget(buttons, 1, 0, 1, 2);

  layout->setColumnStretch(0, 0);
  layout->setColumnStretch(1, 1);
  layout->setRowStretch(0, 1);
  layout->setRowStretch(1, 0);

  loadProfiles();
  QObject::connect(knownProfiles->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(profileSelected(QModelIndex)));

  resize(minimumSizeHint());

  for (int i = 0; i < tabs->count(); i++) {
    for (QObject* w : tabs->widget(i)->children()) {
      if (qobject_cast<QLineEdit*>(w)) {
        QObject::connect(w, SIGNAL(textEdited(QString)), this, SLOT(markDirty()));
      }
    }
  }
  QObject::connect(tTriggers, SIGNAL(markDirty()), this, SLOT(markDirty()));

  QSettings settings;
  restoreGeometry(settings.value("profiles").toByteArray());

  QObject::connect(msspButton, SIGNAL(clicked()), this, SLOT(checkMssp()));
  toggleServerOrProgram();
}

ProfileDialog::ProfileDialog(ProfileDialog::Tab openTab, QWidget* parent)
: ProfileDialog(false, parent)
{
  tabs->setCurrentIndex((int)openTab);
}

void ProfileDialog::loadProfiles()
{
  QSettings settings;
  QString lastProfile = settings.value("lastProfile").toString();
  if (!lastProfile.isEmpty()) {
    lastProfile = "/" + lastProfile + ".galosh";
  }

  QDir dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  QModelIndex lastIdx;
  for (const QString& filename : dir.entryList({ "*.galosh" }, QDir::Files | QDir::Readable)) {
    QString path = dir.absoluteFilePath(filename);
    QSettings settings(path, QSettings::IniFormat);
    if (settings.status() == QSettings::NoError) {
      settings.beginGroup("Profile");
      QStandardItem* item = new QStandardItem();
      item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
      item->setData(settings.value("name", "[invalid profile]"), Qt::DisplayRole);
      item->setData(path, Qt::UserRole);
      profileList->appendRow(item);
      if (path.endsWith(lastProfile)) {
        lastIdx = profileList->indexFromItem(item);
      }
    }
  }
  if (!lastIdx.isValid()) {
    lastIdx = profileList->index(0, 0);
  }
  auto sel = knownProfiles->selectionModel();
  sel->setCurrentIndex(lastIdx, QItemSelectionModel::ClearAndSelect);
  profileSelected(lastIdx);
}

void ProfileDialog::profileSelected(const QModelIndex& current)
{
  tabs->setEnabled(current.isValid());
  buttons->button(QDialogButtonBox::Ok)->setEnabled(current.isValid());
  loadProfile(current.data(Qt::UserRole).toString());
}

void ProfileDialog::saveAndClose()
{
  if (save()) {
    accept();
  }
}

void ProfileDialog::closePromptUnsaved()
{
  reject();
}

void ProfileDialog::loadProfileOffline()
{
  QModelIndex idx = knownProfiles->currentIndex();
  if (idx.isValid() && save()) {
    QSettings settings;
    QDir dir(idx.data(Qt::UserRole).toString());
    settings.setValue("lastProfile", dir.dirName().replace(".galosh", ""));
    emit connectToProfile(idx.data(Qt::UserRole).toString(), false);
    reject();
  }
}

bool ProfileDialog::save()
{
  if (!dirty) {
    return true;
  }

  if (profileName->text().isEmpty()) {
    QMessageBox::critical(this, "Galosh", "Profile name is required.");
    return false;
  }
  QStandardItem* item = profileList->itemFromIndex(knownProfiles->selectionModel()->currentIndex());
  QString path = item->data(Qt::UserRole).toString();
  bool saveError = false;
  if (path.isEmpty()) {
    QDir dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QString name = profileName->text().trimmed();
    name = name.replace(QRegularExpression("[^A-Za-z0-9 _-]"), "").simplified().replace(" ", "_");
    if (name.isEmpty()) {
      name = "profile";
    }
    int i = 1;
    QString filename = name + ".galosh";
    while (dir.exists(filename)) {
      i++;
      filename = QStringLiteral("%1_%2.galosh").arg(name).arg(i);
    }
    if (!dir.mkpath(".")) {
      saveError = true;
    } else {
      path = dir.absoluteFilePath(filename);
      item->setData(path, Qt::UserRole);
    }
  }
  QSettings settings(path, QSettings::IniFormat);
  if (!saveError) {
    settings.sync();
    saveError = !settings.isWritable() || settings.status() != QSettings::NoError;
  }
  if (!saveError) {
    settings.beginGroup("Profile");
    settings.setValue("name", profileName->text().trimmed());
    if (oServer->isChecked()) {
      settings.setValue("host", hostname->text().trimmed());
      settings.setValue("port", port->text().toInt());
      settings.remove("commandLine");
    } else {
      settings.remove("host");
      settings.remove("port");
      settings.setValue("commandLine", hostname->text().trimmed());
    }
    settings.setValue("username", username->text().trimmed());
    settings.setValue("password", password->text());
    settings.setValue("loginPrompt", loginPrompt->text().trimmed());
    settings.setValue("passwordPrompt", passwordPrompt->text().trimmed());
    settings.endGroup();
    saveError = !settings.isWritable() || settings.status() != QSettings::NoError;
  }
  if (saveError) {
    QMessageBox::critical(this, "Galosh", QStringLiteral("Could not save %1").arg(path));
  } else {
    item->setData(profileName->text().trimmed(), Qt::DisplayRole);
    tTriggers->save(path);
  }
  emit profileUpdated(path);
  return true;
}

void ProfileDialog::done(int r)
{
  if (r == QDialog::Accepted) {
    if (!save()) {
      return;
    }
    if (emitConnect) {
      QModelIndex idx = knownProfiles->currentIndex();
      if (!idx.isValid()) {
        qDebug() << "No profile selected";
      } else {
        QSettings settings;
        QDir dir(idx.data(Qt::UserRole).toString());
        settings.setValue("lastProfile", dir.dirName().replace(".galosh", ""));
        emit connectToProfile(idx.data(Qt::UserRole).toString());
      }
    }
  }
  QDialog::done(r);
  deleteLater();
}

void ProfileDialog::newProfile()
{
  QStandardItem* item = new QStandardItem();
  item->setData("(new profile)", Qt::DisplayRole);
  profileList->appendRow(item);
  QModelIndex index = profileList->indexFromItem(item);

  auto sel = knownProfiles->selectionModel();
  sel->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
  profileSelected(index);

  tabs->setCurrentIndex(0);
  profileName->clear();
  hostname->clear();
  port->setText("4000");
  username->clear();
  password->clear();
  loginPrompt->setText(defaultLoginPrompt);
  passwordPrompt->setText(defaultPasswordPrompt);

  profileName->setFocus();
  dirty = true;
}

void ProfileDialog::deleteProfile()
{
  QStandardItem* item = profileList->itemFromIndex(knownProfiles->selectionModel()->currentIndex());
  auto button = QMessageBox::question(this, "Galosh", QStringLiteral("Are you sure you want to delete the profile \"%1\"?").arg(item->data(Qt::DisplayRole).toString()));
  if (button == QMessageBox::Yes) {
    QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty()) {
      QFile::remove(path);
      if (QFile::exists(path)) {
        QMessageBox::critical(this, "Galosh", QStringLiteral("The profile \"%1\" could not be deleted.").arg(item->data(Qt::DisplayRole).toString()));
        return;
      }
    }
    profileList->removeRow(item->row());
  }
}

void ProfileDialog::markDirty()
{
  dirty = true;
}

bool ProfileDialog::loadProfile(const QString& path)
{
  QSettings settings(path, QSettings::IniFormat);
  if (settings.status() != QSettings::NoError) {
    QMessageBox::critical(this, "Galosh", "Error reading profile from " + path);
    return false;
  }
  settings.beginGroup("Profile");
  profileName->setText(settings.value("name").toString());
  QString command = settings.value("commandLine").toString();
  if (command.isEmpty()) {
    oServer->setChecked(true);
    hostname->setText(settings.value("host").toString());
    port->setText(QString::number(settings.value("port").toInt()));
  } else {
    oProgram->setChecked(true);
    hostname->setText(command);
    port->clear();
  }
  username->setText(settings.value("username").toString());
  password->setText(settings.value("password").toString());
  loginPrompt->setText(settings.value("loginPrompt").toString());
  passwordPrompt->setText(settings.value("passwordPrompt").toString());
  toggleServerOrProgram();

  tTriggers->load(path);
  return true;
}

void ProfileDialog::moveEvent(QMoveEvent*)
{
  resizeEvent(nullptr);
}

void ProfileDialog::resizeEvent(QResizeEvent*)
{
  if (isVisible()) {
    QSettings settings;
    settings.setValue("profiles", saveGeometry());
  }
}

void ProfileDialog::checkMssp()
{
  (new MsspView(hostname->text(), port->text().toInt(), this))->open();
}

void ProfileDialog::toggleServerOrProgram()
{
  bool server = oServer->isChecked();
  hostLabel->setText(server ? "&Hostname:" : "Pro&gram:");
  port->setEnabled(server);
  portLabel->setEnabled(server);
  msspButton->setEnabled(server);
  dirty = true;
}
