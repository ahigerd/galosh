#ifndef GALOSH_GALOSHSESSION_H
#define GALOSH_GALOSHSESSION_H

#include <QPointer>
#include <QStringList>
#include <memory>
#include "galoshterm.h"
#include "profiledialog.h"
#include "explorehistory.h"
#include "automapper.h"
#include "userprofile.h"
#include "itemdatabase.h"
#include "commands/textcommandprocessor.h"
class MapManager;
class MapRoom;
class TriggerManager;
class InfoModel;
class ExploreDialog;

class GaloshSession : public QObject, public TextCommandProcessor
{
Q_OBJECT
public:
  GaloshSession(UserProfile* profile, QWidget* parent = nullptr);
  ~GaloshSession();

  std::unique_ptr<UserProfile> profile;
  QPointer<GaloshTerm> term;
  QString name() const;
  MapManager* map() const;
  ItemDatabase* itemDB() const;
  TriggerManager* triggers() const;
  QHash<QString, QString> msspData() const;
  const MapRoom* currentRoom() const;
  inline bool isConnected() const { return !connectedHost().isEmpty(); }
  QString connectedHost() const;
  QString statusBarText() const;

  void connect();
  void startOffline();
  void reload();

  bool hasUnread() const;

  InfoModel* infoModel;

  bool eventFilter(QObject* obj, QEvent* event);

  void openEquipment();
  void switchEquipment(const QString& set);
  void equipmentReceived(const QList<ItemDatabase::EquipSlot>& equipment);

signals:
  void msspReceived();
  void statusUpdated();
  void currentRoomUpdated();
  void openProfileDialog(ProfileDialog::Tab);
  void unreadUpdated();

public slots:
  void exploreMap(int roomId = -1, const QString& movement = QString());
#ifdef Q_MOC_RUN
  void help();

private slots:
  void handleCommand(const QString& command, const QStringList& args);
#endif

private slots:
  void showCommandMessage(TextCommand* command, const QString& message, bool isError) override;
  void speedwalk(const QStringList& steps);
  void abortSpeedwalk();
  void setLastRoom(int roomId);
  void gmcpEvent(const QString& key, const QVariant& value);
  void setUnread();
  void serverCertificate(const QMap<QString, QString>& info, bool selfSigned, bool nameMismatch);

private:
  AutoMapper autoMap;
  ExploreHistory exploreHistory;
  QStringList speedPath;
  QString statusBar;
  QPointer<ExploreDialog> explore;
  bool unread;
};

#endif
