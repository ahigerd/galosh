#ifndef GALOSH_PROFILEDIALOG_H
#define GALOSH_PROFILEDIALOG_H

#include <QDialog>
#include <QModelIndex>
class QTabWidget;
class QListView;
class QFrame;
class QStandardItemModel;
class QDialogButtonBox;
class ServerTab;
class DialogTabBase;

class ProfileDialog : public QDialog
{
Q_OBJECT
public:
  enum Tab {
    Tab_Server,
    Tab_Triggers,
    Tab_Appearance,
    NumTabs,
  };

  static QFrame* horizontalLine(QWidget* parent = nullptr);

  ProfileDialog(bool forConnection, QWidget* parent = nullptr);
  ProfileDialog(ProfileDialog::Tab openTab, QWidget* parent = nullptr);

  void selectProfile(const QString& path);

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

private:
  void loadProfiles();
  bool loadProfile(const QString& path);

  QListView* knownProfiles;
  QStandardItemModel* profileList;
  QTabWidget* tabs;
  QModelIndex selectedProfile;

  ServerTab* tServer;
  QList<DialogTabBase*> tWidgets;
  QDialogButtonBox* buttons;
  bool emitConnect;
  bool dirty;

  static QString lastProfile;
};

#endif
