#ifndef GALOSH_PROFILEDIALOG_H
#define GALOSH_PROFILEDIALOG_H

#include <QDialog>
class QTabWidget;
class QFormLayout;
class QListView;
class QLineEdit;
class QRadioButton;
class QLabel;
class QFrame;
class QStandardItemModel;
class QDialogButtonBox;
class TriggerTab;
class AppearanceTab;

class ProfileDialog : public QDialog
{
Q_OBJECT
public:
  enum Tab {
    ServerTab,
    TriggersTab,
  };

  static QFrame* horizontalLine(QWidget* parent = nullptr);

  ProfileDialog(bool forConnection, QWidget* parent = nullptr);
  ProfileDialog(ProfileDialog::Tab openTab, QWidget* parent = nullptr);

  void done(int r) override;

protected:
  void moveEvent(QMoveEvent*) override;
  void resizeEvent(QResizeEvent*) override;

signals:
  void connectToProfile(const QString& path, bool online = true);
  void profileUpdated(const QString& path);

private slots:
  void profileSelected(const QModelIndex&);
  bool save();
  void saveAndClose();
  void closePromptUnsaved();
  void loadProfileOffline();
  void markDirty();
  void newProfile();
  void deleteProfile();
  void checkMssp();
  void toggleServerOrProgram();

private:
  void loadProfiles();
  bool loadProfile(const QString& path);

  QListView* knownProfiles;
  QStandardItemModel* profileList;
  QTabWidget* tabs;

  QLineEdit* profileName;
  QRadioButton* oServer;
  QRadioButton* oProgram;
  QLineEdit* hostname;
  QLabel* hostLabel;
  QLineEdit* port;
  QLabel* portLabel;
  QPushButton* msspButton;
  QLineEdit* username;
  QLineEdit* password;
  QLineEdit* loginPrompt;
  QLineEdit* passwordPrompt;
  TriggerTab* tTriggers;
  AppearanceTab* tAppearance;
  QDialogButtonBox* buttons;
  bool emitConnect;
  bool dirty;
};

#endif
