#ifndef GALOSH_PROFILEDIALOG_H
#define GALOSH_PROFILEDIALOG_H

#include <QDialog>
class QTabWidget;
class QListView;
class QLineEdit;
class QStandardItemModel;
class QDialogButtonBox;
class TriggerTab;

class ProfileDialog : public QDialog
{
Q_OBJECT
public:
  ProfileDialog(bool forConnection, QWidget* parent = nullptr);

  void done(int r) override;

signals:
  void connectToProfile(const QString& path);

private slots:
  void profileSelected(const QModelIndex&);
  bool save();
  void saveAndClose();
  void closePromptUnsaved();
  void markDirty();
  void newProfile();

private:
  void loadProfiles();
  bool loadProfile(const QString& path);

  QListView* knownProfiles;
  QStandardItemModel* profileList;
  QTabWidget* tabs;
  QLineEdit* profileName;
  QLineEdit* hostname;
  QLineEdit* port;
  QLineEdit* username;
  QLineEdit* password;
  QLineEdit* loginPrompt;
  QLineEdit* passwordPrompt;
  TriggerTab* tTriggers;
  QDialogButtonBox* buttons;
  bool emitConnect;
  bool dirty;
};

#endif
