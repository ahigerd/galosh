#include "profiledialog.h"
#include "userprofile.h"
#include "servertab.h"
#include "triggertab.h"
#include "commandtab.h"
#include "appearancetab.h"
#include "msspview.h"
#include "telnetsocket.h"
#include "algorithms.h"
#include <QMessageBox>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSettings>
#include <QTabWidget>
#include <QListView>
#include <QStandardItemModel>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QBoxLayout>
#include <QGroupBox>
#include <QToolButton>
#include <QDir>
#include <QtDebug>

QString ProfileDialog::lastProfile;

QFrame* ProfileDialog::horizontalLine(QWidget* parent)
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
  tServer = new ServerTab(tabs);
  tWidgets << tServer;
  tWidgets << new TriggerTab(tabs);
  tWidgets << new CommandTab(tabs);
  tWidgets << new AppearanceTab(tabs);

  for (auto [i, tab] : enumerate(tWidgets)) {
    tabs->addTab(tab, QStringLiteral("&%1. %2").arg(i + 1).arg(tab->label()));
    QObject::connect(tab, SIGNAL(markDirty()), this, SLOT(markDirty()));
  }
  layout->addWidget(tabs, 0, 1);

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
  QObject::connect(knownProfiles, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(accept()));

  resize(minimumSizeHint());

  QSettings settings;
  restoreGeometry(settings.value("profiles").toByteArray());

  dirty = false;
}

ProfileDialog::ProfileDialog(ProfileDialog::Tab openTab, QWidget* parent)
: ProfileDialog(false, parent)
{
  tabs->setCurrentIndex((int)openTab);
}

void ProfileDialog::loadProfiles()
{
  QSettings settings;
  if (lastProfile.isEmpty()) {
    lastProfile = settings.value("lastProfile").toString();
  }
  QString mruProfile = QStringLiteral("/%1.galosh").arg(lastProfile);

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
      if (path.endsWith(mruProfile)) {
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

void ProfileDialog::selectProfile(const QString& path)
{
  for (int i = 0; i < profileList->rowCount(); i++) {
    QModelIndex idx = profileList->index(i, 0);
    if (idx.data(Qt::UserRole).toString() == path) {
      knownProfiles->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
      return;
    }
  }
}

void ProfileDialog::profileSelected(const QModelIndex& current)
{
  if (current == selectedProfile) {
    return;
  }
  if (dirty) {
    QString message = QStringLiteral("There are unsaved changes to the profile \"%1\". Save them now?").arg(tServer->profileName());
    int button = QMessageBox::question(this, "Galosh", message, QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    bool cancel = (button == QMessageBox::Cancel);
    if (button == QMessageBox::Save) {
      cancel = !save();
    }
    if (cancel) {
      auto sel = knownProfiles->selectionModel();
      sel->setCurrentIndex(selectedProfile, QItemSelectionModel::ClearAndSelect);
      return;
    }
  }
  selectedProfile = current;
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
  if (selectedProfile.isValid() && save()) {
    QSettings settings;
    QDir dir(selectedProfile.data(Qt::UserRole).toString());
    lastProfile = dir.dirName().replace(".galosh", "");
    settings.setValue("lastProfile", lastProfile);
    emit connectToProfile(selectedProfile.data(Qt::UserRole).toString(), false);
    reject();
  }
}

bool ProfileDialog::save()
{
  if (!dirty) {
    return true;
  }

  QString profileName = tServer->profileName();
  if (profileName.isEmpty()) {
    QMessageBox::critical(this, "Galosh", "Profile name is required.");
    return false;
  }
  QStandardItem* item = profileList->itemFromIndex(selectedProfile);
  QString path = item->data(Qt::UserRole).toString();
  bool saveError = false;
  bool isNew = path.isEmpty();
  if (isNew) {
    QDir dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QString name = profileName.replace(QRegularExpression("[^A-Za-z0-9 _-]"), "").simplified().replace(" ", "_");
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
    UserProfile profile(path);
    saveError = !tServer->save(&profile);
  }
  if (saveError) {
    QMessageBox::critical(this, "Galosh", QStringLiteral("Could not save %1").arg(path));
  } else {
    item->setData(tServer->profileName(), Qt::DisplayRole);
    bool ok = true;
    UserProfile profile(path);
    for (int i = 1; i < tWidgets.length(); i++) {
      ok = tWidgets[i]->save(&profile) && ok;
    }
    if (!ok) {
      return false;
    }
    dirty = false;
    profile.save();
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
      if (!selectedProfile.isValid()) {
        qDebug() << "No profile selected";
      } else {
        QSettings settings;
        QDir dir(selectedProfile.data(Qt::UserRole).toString());
        lastProfile = dir.dirName().replace(".galosh", "");
        settings.setValue("lastProfile", lastProfile);
        emit connectToProfile(selectedProfile.data(Qt::UserRole).toString());
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
  tServer->newProfile();
  tServer->setFocus();
  markDirty();
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
  UserProfile profile(path);
  if (profile.hasLoadError()) {
    QMessageBox::critical(this, "Galosh", "Error reading profile from " + path);
    dirty = false;
    return false;
  }
  for (DialogTabBase* tab : tWidgets) {
    tab->load(&profile);
  }
  dirty = false;
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
